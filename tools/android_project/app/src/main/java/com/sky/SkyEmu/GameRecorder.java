package com.sky.SkyEmu;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.media.Image;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Locale;
import java.util.concurrent.ArrayBlockingQueue;

/*
 * Records the emulated screen and sound to an MP4 (H.264 + AAC). Frames arrive from the emulation thread through
 * submit() and are encoded on a background thread, so a slow encoder drops video frames instead of stalling the
 * game. Timestamps come from the emulated frame count, so the video plays at normal speed even if it was recorded
 * while fast forwarding.
 */
final class GameRecorder {
    interface Listener {
        // location is a user readable place ("Movies/SkyEmu/...") or null when error is set
        void onRecordingFinished(String location, String error);
    }

    private static final String TAG = "SkyEmuRecorder";
    private static final int QUEUE_SIZE = 8;
    private static final long TIMEOUT_US = 10000;

    private static final class Job {
        byte[] rgba;      // null for an audio-only job (video frame dropped) or the end of stream
        short[] pcm;      // interleaved stereo
        boolean end;
    }

    private final Context context;
    private final Listener listener;
    private final int srcW, srcH, scale, outW, outH, sampleRate;
    private final float fps;
    private final File file;
    private final String displayName;
    private final ArrayBlockingQueue<Job> queue = new ArrayBlockingQueue<>(QUEUE_SIZE);
    private final MediaCodec video;
    private final MediaCodec audio;
    private final MediaMuxer muxer;
    private final MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
    private final ArrayList<Object[]> pending = new ArrayList<>(); // {track, data, BufferInfo} until the muxer starts
    private final byte[] yRow, uRow, vRow;
    private final byte[] srcY, srcU, srcV;
    private int videoTrack = -1, audioTrack = -1;
    private boolean muxerStarted, videoDone, audioDone;
    private long videoFrames, audioFrames;
    private volatile boolean failed;
    private volatile boolean finishing;
    private final Thread thread;

    static GameRecorder start(Context context, int width, int height, int scale, int sampleRate, float fps,
                              Listener listener) throws IOException {
        return new GameRecorder(context, width, height, scale, sampleRate, fps, listener);
    }

    private GameRecorder(Context context, int width, int height, int scale, int sampleRate, float fps,
                         Listener listener) throws IOException {
        this.context = context.getApplicationContext();
        this.listener = listener;
        this.srcW = width;
        this.srcH = height;
        this.scale = scale;
        this.outW = width * scale;
        this.outH = height * scale;
        this.sampleRate = sampleRate;
        this.fps = fps;
        displayName = "SkyEmu_" + new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(new Date()) + ".mp4";
        File dir = Build.VERSION.SDK_INT >= 29 ? context.getCacheDir()
                : context.getExternalFilesDir(Environment.DIRECTORY_MOVIES);
        if (dir == null) dir = context.getCacheDir();
        file = new File(dir, displayName);
        yRow = new byte[outW];
        uRow = new byte[outW / 2];
        vRow = new byte[outW / 2];
        srcY = new byte[srcW * srcH];
        srcU = new byte[srcW * srcH];
        srcV = new byte[srcW * srcH];

        MediaFormat vf = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, outW, outH);
        vf.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible);
        vf.setInteger(MediaFormat.KEY_BIT_RATE, Math.max(2_000_000, outW * outH * 12));
        vf.setInteger(MediaFormat.KEY_FRAME_RATE, Math.round(fps));
        vf.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 2);
        MediaFormat af = MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC, sampleRate, 2);
        af.setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC);
        af.setInteger(MediaFormat.KEY_BIT_RATE, 160_000);
        af.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 16384);

        MediaCodec v = null, a = null;
        try {
            v = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
            v.configure(vf, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            a = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_AUDIO_AAC);
            a.configure(af, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            muxer = new MediaMuxer(file.getPath(), MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            v.start();
            a.start();
        } catch (IOException | RuntimeException e) {
            if (v != null) v.release();
            if (a != null) a.release();
            file.delete();
            throw e instanceof IOException ? (IOException) e : new IOException(e);
        }
        video = v;
        audio = a;
        thread = new Thread(this::run, "SkyEmuRecorder");
        thread.start();
    }

    // Called from the emulation thread. Copies the data, so the caller may reuse its buffers.
    boolean submit(ByteBuffer rgba, ByteBuffer pcm) {
        if (failed || finishing) return false;
        Job job = new Job();
        if (rgba != null && queue.remainingCapacity() > 1) {
            job.rgba = new byte[srcW * srcH * 4];
            rgba.position(0);
            rgba.get(job.rgba, 0, Math.min(job.rgba.length, rgba.remaining()));
        }
        if (pcm != null && pcm.capacity() >= 4) {
            pcm.position(0);
            job.pcm = new short[pcm.capacity() / 2];
            pcm.order(ByteOrder.nativeOrder()).asShortBuffer().get(job.pcm);
        }
        try {
            queue.put(job); // only blocks when the queue is full of audio, which is tiny
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return false;
        }
        return !failed;
    }

    // Finishes the file on the encoder thread, then publishes it and calls the listener.
    void finish() {
        if (finishing) return;
        finishing = true;
        Job end = new Job();
        end.end = true;
        while (true) {
            try {
                queue.put(end);
                return;
            } catch (InterruptedException ignored) {
            }
        }
    }

    private void run() {
        String location = null, error = null;
        try {
            // A short block of silence makes the audio encoder report its format even if the game is silent.
            encodeAudio(new short[2048]);
            while (true) {
                Job job = queue.take();
                if (job.end) break;
                if (job.rgba != null) encodeVideo(job.rgba);
                videoFrames++; // a dropped frame keeps its time slot, the previous image stays on screen
                if (job.pcm != null) encodeAudio(job.pcm);
                drain(video, false);
                drain(audio, false);
            }
            endStream(video, (long) (videoFrames * 1e6 / fps));
            endStream(audio, audioFrames * 1_000_000L / sampleRate);
            for (int i = 0; i < 500 && !(videoDone && audioDone); ++i) {
                drain(video, true);
                drain(audio, true);
            }
            if (!muxerStarted) throw new IOException("The encoder produced no video");
            muxer.stop();
            location = publish();
        } catch (Exception e) {
            failed = true;
            error = e.getMessage() != null ? e.getMessage() : e.toString();
            Log.e(TAG, "Recording failed", e);
            file.delete();
        } finally {
            safeRelease();
        }
        if (listener != null) listener.onRecordingFinished(location, error);
    }

    private static int clamp(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

    // BT.601 limited range, computed once per source pixel and replicated for the integer upscale.
    private void encodeVideo(byte[] rgba) {
        for (int i = 0, p = 0; i < srcW * srcH; ++i, p += 4) {
            int r = rgba[p] & 0xff, g = rgba[p + 1] & 0xff, b = rgba[p + 2] & 0xff;
            srcY[i] = (byte) clamp(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
            srcU[i] = (byte) clamp(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
            srcV[i] = (byte) clamp(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
        }
        int index = video.dequeueInputBuffer(TIMEOUT_US);
        if (index < 0) return; // encoder busy: drop this frame
        Image image = video.getInputImage(index);
        if (image == null) {
            video.queueInputBuffer(index, 0, 0, (long) (videoFrames * 1e6 / fps), 0);
            return;
        }
        Image.Plane[] planes = image.getPlanes();
        writePlane(planes[0], srcY, 1);
        writePlane(planes[1], srcU, 2);
        writePlane(planes[2], srcV, 2);
        video.queueInputBuffer(index, 0, outW * outH * 3 / 2, (long) (videoFrames * 1e6 / fps), 0);
    }

    private void writePlane(Image.Plane plane, byte[] src, int subsample) {
        ByteBuffer buffer = plane.getBuffer();
        int rowStride = plane.getRowStride();
        int pixelStride = plane.getPixelStride();
        int w = outW / subsample, h = outH / subsample;
        byte[] row = subsample == 1 ? yRow : (src == srcU ? uRow : vRow);
        for (int y = 0; y < h; ++y) {
            int sy = y * subsample / scale;
            for (int x = 0; x < w; ++x) row[x] = src[sy * srcW + x * subsample / scale];
            int base = y * rowStride;
            if (pixelStride == 1) {
                buffer.position(base);
                buffer.put(row, 0, w);
            } else {
                for (int x = 0; x < w; ++x) buffer.put(base + x * pixelStride, row[x]);
            }
        }
    }

    private void encodeAudio(short[] pcm) throws IOException {
        int offset = 0; // in shorts
        int tries = 0;
        while (offset < pcm.length) {
            int index = audio.dequeueInputBuffer(TIMEOUT_US);
            if (index < 0) {
                drain(audio, false);
                if (++tries > 50) throw new IOException("Audio encoder stalled");
                continue;
            }
            ByteBuffer in = audio.getInputBuffer(index);
            in.clear();
            int shorts = Math.min(pcm.length - offset, (in.capacity() / 4) * 2);
            in.order(ByteOrder.nativeOrder()).asShortBuffer().put(pcm, offset, shorts);
            audio.queueInputBuffer(index, 0, shorts * 2, audioFrames * 1_000_000L / sampleRate, 0);
            audioFrames += shorts / 2;
            offset += shorts;
        }
    }

    private void endStream(MediaCodec codec, long ptsUs) {
        for (int i = 0; i < 100; ++i) {
            int index = codec.dequeueInputBuffer(TIMEOUT_US);
            if (index >= 0) {
                codec.queueInputBuffer(index, 0, 0, ptsUs, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                return;
            }
            drain(codec, false);
        }
    }

    private void drain(MediaCodec codec, boolean waiting) {
        boolean isVideo = codec == video;
        if (isVideo ? videoDone : audioDone) return;
        while (true) {
            int index = codec.dequeueOutputBuffer(info, waiting ? TIMEOUT_US : 0);
            if (index == MediaCodec.INFO_TRY_AGAIN_LATER) return;
            if (index == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                int track = muxer.addTrack(codec.getOutputFormat());
                if (isVideo) videoTrack = track; else audioTrack = track;
                if (videoTrack >= 0 && audioTrack >= 0) startMuxer();
                continue;
            }
            if (index < 0) continue;
            ByteBuffer out = codec.getOutputBuffer(index);
            boolean config = (info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0;
            if (out != null && info.size > 0 && !config) {
                int track = isVideo ? videoTrack : audioTrack;
                if (muxerStarted) {
                    muxer.writeSampleData(track, out, info);
                } else {
                    // The other encoder has not reported its format yet: keep the sample for later.
                    ByteBuffer copy = ByteBuffer.allocateDirect(info.size);
                    out.position(info.offset);
                    out.limit(info.offset + info.size);
                    copy.put(out);
                    copy.flip();
                    MediaCodec.BufferInfo saved = new MediaCodec.BufferInfo();
                    saved.set(0, info.size, info.presentationTimeUs, info.flags);
                    pending.add(new Object[]{isVideo, copy, saved});
                }
            }
            boolean end = (info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
            codec.releaseOutputBuffer(index, false);
            if (end) {
                if (isVideo) videoDone = true; else audioDone = true;
                return;
            }
        }
    }

    private void startMuxer() {
        muxer.start();
        muxerStarted = true;
        for (Object[] p : pending) {
            int track = (Boolean) p[0] ? videoTrack : audioTrack;
            muxer.writeSampleData(track, (ByteBuffer) p[1], (MediaCodec.BufferInfo) p[2]);
        }
        pending.clear();
    }

    // Moves the finished file where gallery and file manager apps see it.
    private String publish() throws IOException {
        if (Build.VERSION.SDK_INT >= 29) {
            ContentResolver resolver = context.getContentResolver();
            ContentValues values = new ContentValues();
            values.put(MediaStore.Video.Media.DISPLAY_NAME, displayName);
            values.put(MediaStore.Video.Media.MIME_TYPE, "video/mp4");
            values.put(MediaStore.Video.Media.RELATIVE_PATH, Environment.DIRECTORY_MOVIES + "/SkyEmu");
            values.put(MediaStore.Video.Media.IS_PENDING, 1);
            Uri uri = resolver.insert(MediaStore.Video.Media.EXTERNAL_CONTENT_URI, values);
            if (uri == null) throw new IOException("Could not create the video in Movies");
            try (InputStream in = new FileInputStream(file); OutputStream out = resolver.openOutputStream(uri)) {
                if (out == null) throw new IOException("Could not write the video");
                byte[] buffer = new byte[64 * 1024];
                int n;
                while ((n = in.read(buffer)) > 0) out.write(buffer, 0, n);
            } catch (IOException e) {
                resolver.delete(uri, null, null);
                throw e;
            }
            values.clear();
            values.put(MediaStore.Video.Media.IS_PENDING, 0);
            resolver.update(uri, values, null, null);
            file.delete();
            return Environment.DIRECTORY_MOVIES + "/SkyEmu/" + displayName;
        }
        MediaScannerConnection.scanFile(context, new String[]{file.getPath()}, new String[]{"video/mp4"}, null);
        return file.getPath();
    }

    private void safeRelease() {
        try { video.stop(); } catch (Exception ignored) { }
        try { audio.stop(); } catch (Exception ignored) { }
        video.release();
        audio.release();
        try { muxer.release(); } catch (Exception ignored) { }
    }
}
