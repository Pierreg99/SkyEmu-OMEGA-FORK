package com.sky.SkyEmu;


import static android.view.InputDevice.SOURCE_GAMEPAD;
import static android.view.InputDevice.SOURCE_JOYSTICK;
import static android.view.KeyEvent.*;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.text.InputType;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.Window;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;

import android.app.NativeActivity;
import android.widget.EditText;
import android.widget.FrameLayout;

import androidx.browser.customtabs.CustomTabsIntent;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.Key;
import java.util.Locale;
import java.util.Vector;

public class EnhancedNativeActivity extends NativeActivity {
    final static int APP_STORAGE_ACCESS_REQUEST_CODE = 501; // Any value
    final static int STORAGE_PERMISSION_CODE = 501; // Any value
    final static int FILE_PICKER_REQUEST_CODE = 123;
    final static String TAG="SkyEmu"; // Any value
    public Rect visibleRect = new Rect();
    public EditText invisibleEditText;
    public View mRootView;
    private Vector<Integer> keyboardEvents;
    private boolean first_event;
    CustomTabsIntent authIntent;

    static {
        System.loadLibrary("SkyEmu");
    }
    public void requestPermissions() {
    }
    public float getDPIScale(){
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
        return metrics.xdpi/120.0f;
    }
    public static String getLanguage() {
        return Locale.getDefault().toString();
    }

    // Material You tonal palettes (Android 12+), ordered like SkyEmu's standard tones:
    // 100, 99, 95, 90, 80, 70, 60, 50, 40, 30, 20, 10, 0
    private static final int[][] MATERIAL_YOU_PALETTES = Build.VERSION.SDK_INT >= Build.VERSION_CODES.S ? new int[][]{
        {android.R.color.system_accent1_0, android.R.color.system_accent1_10, android.R.color.system_accent1_50,
         android.R.color.system_accent1_100, android.R.color.system_accent1_200, android.R.color.system_accent1_300,
         android.R.color.system_accent1_400, android.R.color.system_accent1_500, android.R.color.system_accent1_600,
         android.R.color.system_accent1_700, android.R.color.system_accent1_800, android.R.color.system_accent1_900,
         android.R.color.system_accent1_1000},
        {android.R.color.system_accent2_0, android.R.color.system_accent2_10, android.R.color.system_accent2_50,
         android.R.color.system_accent2_100, android.R.color.system_accent2_200, android.R.color.system_accent2_300,
         android.R.color.system_accent2_400, android.R.color.system_accent2_500, android.R.color.system_accent2_600,
         android.R.color.system_accent2_700, android.R.color.system_accent2_800, android.R.color.system_accent2_900,
         android.R.color.system_accent2_1000},
        {android.R.color.system_accent3_0, android.R.color.system_accent3_10, android.R.color.system_accent3_50,
         android.R.color.system_accent3_100, android.R.color.system_accent3_200, android.R.color.system_accent3_300,
         android.R.color.system_accent3_400, android.R.color.system_accent3_500, android.R.color.system_accent3_600,
         android.R.color.system_accent3_700, android.R.color.system_accent3_800, android.R.color.system_accent3_900,
         android.R.color.system_accent3_1000},
        {android.R.color.system_neutral1_0, android.R.color.system_neutral1_10, android.R.color.system_neutral1_50,
         android.R.color.system_neutral1_100, android.R.color.system_neutral1_200, android.R.color.system_neutral1_300,
         android.R.color.system_neutral1_400, android.R.color.system_neutral1_500, android.R.color.system_neutral1_600,
         android.R.color.system_neutral1_700, android.R.color.system_neutral1_800, android.R.color.system_neutral1_900,
         android.R.color.system_neutral1_1000},
        {android.R.color.system_neutral2_0, android.R.color.system_neutral2_10, android.R.color.system_neutral2_50,
         android.R.color.system_neutral2_100, android.R.color.system_neutral2_200, android.R.color.system_neutral2_300,
         android.R.color.system_neutral2_400, android.R.color.system_neutral2_500, android.R.color.system_neutral2_600,
         android.R.color.system_neutral2_700, android.R.color.system_neutral2_800, android.R.color.system_neutral2_900,
         android.R.color.system_neutral2_1000},
    } : null;

    /*
     * Called from native code to theme the Material 3 GUI.
     * Layout: [dark (1 dark, 0 light, -1 unknown), accent ARGB (0 if unknown), palette count (0 or 5),
     *          then accent1, accent2, accent3, neutral1, neutral2 with 13 tones each]
     */
    public int[] getSystemAppearance() {
        int[] out = new int[3 + 5 * 13];
        int night = getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
        out[0] = night == Configuration.UI_MODE_NIGHT_YES ? 1 : night == Configuration.UI_MODE_NIGHT_NO ? 0 : -1;
        if (MATERIAL_YOU_PALETTES != null) {
            try {
                for (int p = 0; p < 5; ++p) {
                    for (int t = 0; t < 13; ++t) {
                        out[3 + p * 13 + t] = getResources().getColor(MATERIAL_YOU_PALETTES[p][t], getTheme());
                    }
                }
                out[1] = out[3 + 7]; // system_accent1_500
                out[2] = 5;
            } catch (Exception e) {
                Log.w(TAG, "Material You palette unavailable", e);
                out[1] = 0;
                out[2] = 0;
            }
        }
        return out;
    }

    public void setRemoteKeycodeCallback(String keyCode) {
        // empty
    }

    public void openExternalMenu() {
        // empty
    }

    public void ping() {
        // empty
    }
    /*Handle permission request results*/
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == STORAGE_PERMISSION_CODE){
            if (grantResults.length > 0){
                //check each permission if granted or not
                boolean write = grantResults[0] == PackageManager.PERMISSION_GRANTED;
                boolean read = grantResults[1] == PackageManager.PERMISSION_GRANTED;

                if (write && read){
                    //External Storage permissions granted
                    Log.d(TAG, "onRequestPermissionsResult: External Storage permissions granted");
                }
                else{
                    //External Storage permission denied
                    Log.d(TAG, "onRequestPermissionsResult: External Storage permission denied");
                }
            }
        }
    }

    public float getVisibleBottom(){
        return visibleRect.bottom;
    }
    public float getVisibleTop(){
        return visibleRect.top;
    }
    public int getEvent(){
        if(first_event){
            Intent intent = getIntent();
            Uri data = intent.getData();
            if (intent.getAction()==Intent.ACTION_VIEW&&data != null) {
                loadURI(data,true);
            }
            first_event=false;
        }
        if(keyboardEvents.isEmpty())return -1;
        int val = keyboardEvents.get(0);
        keyboardEvents.remove(0);
        return val;
    }
    public void showKeyboard(){
        Window win =this.getWindow();
        NativeActivity activity = this;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if(invisibleEditText==null){
                    FrameLayout.LayoutParams mRparams = new FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT);
                    invisibleEditText = new EditText(activity);
                    invisibleEditText.setLayoutParams(mRparams);
                    invisibleEditText.setRawInputType(InputType.TYPE_CLASS_TEXT);
                    invisibleEditText.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI);
                    // Set an OnKeyListener to intercept key events
                    invisibleEditText.setOnKeyListener(new View.OnKeyListener() {
                        @Override
                        public boolean onKey(View v, int keyCode, KeyEvent event) {
                            // Consume the key event to prevent it from reaching the EditText
                            // So, that we don not duplicate the inputs relayed through the C code for onKey.
                            return true;
                        }
                    });
                    ((FrameLayout)mRootView).addView(invisibleEditText);
                }
                invisibleEditText.requestFocus();
                InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                imm.showSoftInput(invisibleEditText, InputMethodManager.SHOW_IMPLICIT);
            }
        });
    }

    public void hideKeyboard()
    {
        Window win =this.getWindow();
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ((FrameLayout)mRootView).removeView(invisibleEditText);
                invisibleEditText = null;
            }
        });
    }
    public void pollKeyboard(){
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if(invisibleEditText==null)return;
                int pre = 0;
                boolean inserted = false;
                if(invisibleEditText.getSelectionEnd()!= invisibleEditText.getText().length()-8){
                    int distance =  invisibleEditText.getText().length()-invisibleEditText.getSelectionEnd()-8;
                    for(int c=0; c<distance;++c ){
                        //Left Arrow
                        keyboardEvents.add(1| 0x40000000);
                    }
                    distance =  invisibleEditText.getSelectionEnd()-(invisibleEditText.getText().length()-8);
                    for(int c=0; c<distance;++c ){
                        //Right Arrow
                        keyboardEvents.add(2| 0x40000000);
                    }
                }
                for (int c : invisibleEditText.getText().toString().chars().toArray()) {
                    if (pre < 8) {
                        if (c == '\1') {
                            pre++;
                            continue;
                        } else {
                            while (pre < 8) {
                                inserted=true;
                                // Backspace
                                keyboardEvents.add(11 | 0x40000000);
                                pre++;
                            }
                        }
                    }
                    if(pre>=invisibleEditText.getText().length()-8){
                        break;
                    }
                    pre++;
                    inserted=true;
                    //Enter
                    if(c=='\n'){keyboardEvents.add(13 |0x40000000);
                    }else keyboardEvents.add(c);
                }
                while (pre < 8) {
                    inserted=true;
                    keyboardEvents.add(11 | 0x40000000);
                    pre++;
                }
                if(inserted) {
                    invisibleEditText.setText("\1\1\1\1\1\1\1\1\2\2\2\2\2\2\2\2");
                    invisibleEditText.setSelection(invisibleEditText.getText().length()-8);
                }
                if(invisibleEditText.getSelectionEnd()!= invisibleEditText.getText().length()-8)
                    invisibleEditText.setSelection(invisibleEditText.getText().length()-8);
            }
        });
    }
    private File copyFileToExternalDirectory(Uri sourceUri, String directory, String filename) {
        File destinationDirectory = new File(directory);
        if (!destinationDirectory.isDirectory() && !destinationDirectory.mkdirs()) return null;
        // Providers supply display names, not filesystem paths.
        String safeName = filename.replace('\\', '/');
        safeName = safeName.substring(safeName.lastIndexOf('/') + 1);
        if (safeName.isEmpty() || safeName.equals(".") || safeName.equals("..")) safeName = "imported-game";
        File temporary = null;
        try {
            temporary = File.createTempFile("skyemu-import-", ".tmp", destinationDirectory);
            try (InputStream in = getContentResolver().openInputStream(sourceUri);
                 OutputStream out = new FileOutputStream(temporary)) {
                if (in == null) throw new IOException("Provider did not return a file stream");
                byte[] buffer = new byte[64 * 1024];
                int length;
                while ((length = in.read(buffer)) != -1) out.write(buffer, 0, length);
            }
            File copiedFile = new File(destinationDirectory, safeName);
            if (!temporary.renameTo(copiedFile)) throw new IOException("Could not finish file import");
            return copiedFile;
        } catch (IOException | SecurityException e) {
            Log.e("SkyEmu", "File import failed", e);
            return null;
        } finally {
            if (temporary != null && temporary.exists()) temporary.delete();
        }
    }
    protected void onCreate(Bundle savedInstanceState) {
        first_event=true;
        super.onCreate(savedInstanceState);
        Window mRootWindow = getWindow();
        mRootView = mRootWindow.getDecorView().findViewById(android.R.id.content);
        invisibleEditText=null;
        keyboardEvents = new Vector<Integer>(5);

        EnhancedNativeActivity activity = this;
        mRootView.getViewTreeObserver().addOnGlobalLayoutListener(
            new ViewTreeObserver.OnGlobalLayoutListener() {
                public void onGlobalLayout(){
                    Rect r = new Rect();
                    View view = mRootWindow.getDecorView();
                    view.getWindowVisibleDisplayFrame(r);
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                        android.view.WindowInsets insets = view.getRootWindowInsets();
                        if (insets != null) {
                            android.graphics.Insets safe = insets.getInsets(
                                    android.view.WindowInsets.Type.systemBars()
                                    | android.view.WindowInsets.Type.displayCutout()
                                    | android.view.WindowInsets.Type.ime());
                            r.set(safe.left, safe.top, view.getWidth() - safe.right,
                                    view.getHeight() - safe.bottom);
                        }
                    }
                    activity.visibleRect = r;
                }
            });

        int currentApiVersion = Build.VERSION.SDK_INT;

        final int flags = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        mRootWindow.getDecorView().setOnGenericMotionListener(new View.OnGenericMotionListener() {
            @Override
            public boolean onGenericMotion(View view, MotionEvent event) {
                for(int i=0;i<event.getPointerCount();++i){
                    for (InputDevice.MotionRange range: event.getDevice().getMotionRanges()) {
                        int ax = range.getAxis();
                        float v = event.getAxisValue(ax,i);
                        int int_val = (int)(v*32767.);
                        int skyemu_event = 0x10000000|(ax<<16)|(int_val&0xffff);
                        keyboardEvents.add(skyemu_event);
                    }
                }
                // If the event is not the back button press, let it propagate as usual
                return false;
            }
        });
        // This work only for android 4.4+
        if (currentApiVersion >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            android.view.WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.setSystemBarsBehavior(android.view.WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                controller.hide(android.view.WindowInsets.Type.systemBars());
            }
        } else if(currentApiVersion >= Build.VERSION_CODES.KITKAT){
            getWindow().getDecorView().setSystemUiVisibility(flags);

            // Code below is to handle presses of Volume up or Volume down.
            // Without this, after pressing volume buttons, the navigation bar will
            // show up and won't hide
            final View decorView = getWindow().getDecorView();
            decorView
                    .setOnSystemUiVisibilityChangeListener(new View.OnSystemUiVisibilityChangeListener()
                    {
                        @Override
                        public void onSystemUiVisibilityChange(int visibility)
                        {
                            if((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0)
                            {
                                decorView.setSystemUiVisibility(flags);
                            }
                        }
                    });
        }
    }
    public void openFile(){
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("*/*");
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        startActivityForResult(intent, FILE_PICKER_REQUEST_CODE);
    }
    public String getFileName(Uri uri) {
        if (uri == null) return "imported-game";
        String result = null;
        if ("content".equals(uri.getScheme())) {
            try (Cursor cursor = getContentResolver().query(uri, null, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    int column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                    if (column >= 0) result = cursor.getString(column);
                }
            } catch (SecurityException | IllegalArgumentException e) {
                Log.w("SkyEmu", "Could not read provider display name", e);
            }
        }
        if (result == null || result.isEmpty()) result = uri.getLastPathSegment();
        return result == null || result.isEmpty() ? "imported-game" : result;
    }
    @Override
    public boolean onKeyDown(int keycode, KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            return true;
        }
        int gamepad_keycode = event.getKeyCode()&0xffff;

        if((event.getSource() & SOURCE_JOYSTICK)!=SOURCE_JOYSTICK){
            int skyemu_event = gamepad_keycode | 0x20000000;
            if(event.getAction()==ACTION_DOWN)skyemu_event|=(1<<16);
            keyboardEvents.add(skyemu_event);
            if((event.getSource() & SOURCE_GAMEPAD)==SOURCE_GAMEPAD)return true;
        }
        // If the event is not the back button press, let it propagate as usual
        return false;
    }
    @Override
    public boolean onKeyUp(int keycode, KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            return true;
        }
        int gamepad_keycode = event.getKeyCode()&0xffff;

        if((event.getSource() & SOURCE_JOYSTICK)!=SOURCE_JOYSTICK){
            int skyemu_event = gamepad_keycode | 0x20000000;
            if(event.getAction()==ACTION_DOWN)skyemu_event|=(1<<16);
            keyboardEvents.add(skyemu_event);
            if((event.getSource() & SOURCE_GAMEPAD)==SOURCE_GAMEPAD)return true;
        }
        // If the event is not the back button press, let it propagate as usual
        return false;
    }
    public void loadURI(Uri selectedFileUri, boolean is_rom) {
        if (selectedFileUri == null) return;
        File directory = getExternalFilesDir(null);
        if (directory == null) directory = getFilesDir();
        File copiedFile = copyFileToExternalDirectory(selectedFileUri,
                directory.getAbsolutePath(), getFileName(selectedFileUri));
        if (copiedFile != null) {
            if (is_rom) se_android_load_rom(copiedFile.getAbsolutePath());
            else se_android_load_file(copiedFile.getAbsolutePath());
        }
    }
    public void openCustomTab(String url){
        authIntent = new CustomTabsIntent.Builder().build();
        authIntent.intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        authIntent.launchUrl(EnhancedNativeActivity.this, Uri.parse(url));
    }
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        // If the selection didn't work
        if (resultCode != RESULT_OK) {
            // Exit without doing anything else
            return;
        } else {
            if (requestCode == FILE_PICKER_REQUEST_CODE && data != null) {
                Uri selectedFileUri = data.getData();
                loadURI(selectedFileUri,false);
            }
        }
    }
    // Shader IDs 0-7; density 0 comfortable, 1 compact, 2 touch.
    public native void se_android_set_display_effects(float scanlines, float mask, float curvature, float vignette);
    public native void se_android_set_display_color(float brightness, float saturation, float contrast);
    public native void se_android_set_design_options(int highContrast, int density, float roundness);
    public native void se_android_load_file(String filePath);
    public native void se_android_load_rom(String filePath);
    public native void se_android_load_html(String filePath);
    public native void se_android_show_ui(boolean isShow);
    public native void se_android_stretch_on();
    public native void se_android_stretch_off();
    public native void se_android_capture_state_slot(int slot);
    public native void se_android_restore_state_slot(int slot);

    public void LoadFile(String filePath){
        se_android_load_file(filePath);
    }

    public void LoadRom(String filePath){
        se_android_load_rom(filePath);
    }

    public void LoadHtml(String filePath){
        se_android_load_html(filePath);
    }
    
    public void ShowUI(){
        se_android_show_ui(true);
    }

    public void HideUI(){
        se_android_show_ui(false);
    }

    public void StretchOn() {
        se_android_stretch_on();
    }

    public void StretchOff() {
        se_android_stretch_off();
    }
}
