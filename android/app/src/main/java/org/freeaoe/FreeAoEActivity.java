package org.freeaoe;

import org.libsdl.app.SDLActivity;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.content.Intent;
import android.provider.Settings;
import android.net.Uri;
import java.io.File;

public class FreeAoEActivity extends SDLActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Request all files access on Android 11+
        if (Build.VERSION.SDK_INT >= 30 && !Environment.isExternalStorageManager()) {
            Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
            intent.setData(Uri.parse("package:" + getPackageName()));
            startActivity(intent);
        }
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getLibraries() {
        return new String[]{
            "c++_shared",
            "SDL2",
            "SDL2_ttf",
            "freeaoe"
        };
    }

    @Override
    protected String[] getArguments() {
        File dataDir = new File(Environment.getExternalStorageDirectory(),
            "Download/aoe2data");
        android.util.Log.i("FreeAoE", "Data dir: " + dataDir.getAbsolutePath() + " exists: " + dataDir.exists());
        return new String[]{"--game-path=" + dataDir.getAbsolutePath(), "--single-player", "--language=ru"};
    }
}
