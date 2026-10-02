/*****************************************************************************
 *
 *   SkyEmu recording
 *
 *   Writes gameplay videos as AVI files (MJPEG or uncompressed frames with
 *   PCM audio, playable by VLC, mpv, browsers' download players, Windows and
 *   video editors), audio as WAV files, and keeps a replay buffer of the last
 *   seconds of play that can be saved as a video. Only depends on
 *   stb_image_write for JPEG, so it can be unit tested on its own (see
 *   tools/se_record_test.c).
 *
**/
#ifndef SE_RECORD_H
#define SE_RECORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
  uint8_t* data;
  size_t size, capacity;
}se_record_buffer_t;

bool se_record_buffer_reserve(se_record_buffer_t* buffer, size_t capacity);
void se_record_buffer_free(se_record_buffer_t* buffer);

// Scales an RGBA image by an integer factor (nearest neighbor, keeps pixel art sharp) and encodes
// it into out: a JPEG of the given quality (1-100, above 90 colors are not subsampled), or for
// quality 0 uncompressed bottom-up BGR rows padded to 4 bytes, as stored in AVI files. scratch
// holds the scaled image between calls. Returns false when out of memory.
bool se_record_encode_frame(const uint8_t* rgba, int width, int height, int scale, int quality,
                            se_record_buffer_t* scratch, se_record_buffer_t* out);

// Path of a later part of a recording that was split: "a/b.avi", 2 -> "a/b (2).avi"
void se_record_part_path(const char* path, int part, char* out, size_t out_size);

typedef struct{
  int width, height;          // Size of the encoded frames
  int quality;                // 1-100 for MJPEG frames, 0 for uncompressed frames
  uint32_t fps_num, fps_den;  // Frame rate as a fraction, for example 16777216 / 280896 for the GBA
  uint32_t audio_rate;        // Samples per second, 0 for a video without sound
  uint32_t audio_channels;    // 16 bit interleaved samples
  uint64_t split_size;        // A file is continued in a new part at this size, 0 for 1 GiB
}se_avi_format_t;

// AVI 1.0 writer. Frames are added already encoded (se_record_encode_frame) with the audio that
// belongs to them. Files close to split_size are finished and the recording continues in
// "<name> (2).avi" and so on, which every player and editor can open.
typedef struct se_avi_writer_t se_avi_writer_t;
se_avi_writer_t* se_avi_open(const char* path, const se_avi_format_t* format);
bool se_avi_add_audio(se_avi_writer_t* avi, const int16_t* samples, size_t frames);
bool se_avi_add_frame(se_avi_writer_t* avi, const uint8_t* data, size_t size);
// Writes the index and sizes, closes the file and frees the writer. False if anything failed.
bool se_avi_close(se_avi_writer_t* avi);
uint64_t se_avi_frames(const se_avi_writer_t* avi);   // Video frames written, all parts
uint64_t se_avi_bytes(const se_avi_writer_t* avi);    // Bytes written so far, all parts (the index comes on close)
int se_avi_parts(const se_avi_writer_t* avi);
const char* se_avi_error(const se_avi_writer_t* avi); // NULL while all writes succeeded

// WAV writer for 16 bit PCM, split like the AVI writer at 2 GiB
typedef struct se_wav_writer_t se_wav_writer_t;
se_wav_writer_t* se_wav_open(const char* path, uint32_t rate, uint32_t channels);
bool se_wav_add_audio(se_wav_writer_t* wav, const int16_t* samples, size_t frames);
bool se_wav_close(se_wav_writer_t* wav);
uint64_t se_wav_frames(const se_wav_writer_t* wav);   // Sample frames written
const char* se_wav_error(const se_wav_writer_t* wav);

// Replay buffer: the last frames played, encoded, with their audio. When it is full the oldest
// frame is replaced.
typedef struct{
  se_record_buffer_t video;
  int16_t* audio;
  uint32_t audio_frames, audio_capacity;
}se_replay_frame_t;

typedef struct{
  se_replay_frame_t* frames;
  uint32_t capacity, count, next;
  uint32_t audio_channels;
  int width, height, quality;  // Format of the stored frames
}se_replay_buffer_t;

bool se_replay_init(se_replay_buffer_t* replay, uint32_t capacity, int width, int height, int quality,
                    uint32_t audio_channels);
void se_replay_free(se_replay_buffer_t* replay);
void se_replay_clear(se_replay_buffer_t* replay);
bool se_replay_push(se_replay_buffer_t* replay, const uint8_t* data, size_t size, const int16_t* audio,
                    uint32_t audio_frames);
// Writes the frames from the oldest to the newest as an AVI file. format must match the frames.
bool se_replay_save(const se_replay_buffer_t* replay, const char* path, const se_avi_format_t* format);

#ifdef __cplusplus
}
#endif

#endif
