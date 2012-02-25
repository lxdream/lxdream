/**
 * $Id$
 * 
 * Main Lxdream activity 
 *
 * Copyright (c) 2011 Nathan Keynes.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

package org.lxdream;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;

import java.io.File;


public class LxdreamActivity extends Activity {
    LxdreamView view;

    @Override 
    protected void onCreate(Bundle bundle) {
        super.onCreate(bundle);
        view = new LxdreamView(getApplication());
        setContentView(view);
    }

    @Override 
    protected void onPause() {
        super.onPause();
        view.onPause();
    }

    @Override 
    protected void onResume() {
        super.onResume();
        view.onResume();
    }
}
