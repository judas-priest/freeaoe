package org.freeaoe;

import org.libsdl.app.SDLActivity;
import android.os.Environment;
import java.io.File;

public class FreeAoEActivity extends SDLActivity {
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
        File dataDir = new File(getExternalFilesDir(null), "aoe2data");
        if (!dataDir.exists()) {
            dataDir = new File(Environment.getExternalStorageDirectory(),
                "Download/aoe2data");
        }
        return new String[]{"--game-path", dataDir.getAbsolutePath()};
    }
}
