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
import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.WindowManager;

import java.io.File;


public class LxdreamActivity extends Activity {
    LxdreamView view;
    boolean isRunning = false;
    Context ctx;
    Drawable runIcon, pauseIcon;
    MenuItem runMenuItem;

    @Override 
    protected void onCreate(Bundle bundle) {
        super.onCreate(bundle);
        ctx = getApplication();
        Resources res = ctx.getResources();
        runIcon = res.getDrawable(R.drawable.tb_run);
        pauseIcon = res.getDrawable(R.drawable.tb_pause);
        
        Log.i("LxdreamActivity", "Calling Dreamcast.init");
        Dreamcast.init( Environment.getExternalStorageDirectory().toString() + "/lxdream" );
        Log.i("LxdreamActivity", "Finished Dreamcast.init");
        view = new LxdreamView(ctx);
        setContentView(view);
    }
    
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.main, menu);
        runMenuItem = menu.findItem(R.id.menu_run);
        return true;
    }


    @Override 
    protected void onPause() {
        Dreamcast.stop();
        runMenuItem.setIcon( runIcon );
        isRunning = false;
        super.onPause();
    }

    @Override 
    protected void onResume() {
        super.onResume();
    }
    
    public void onRunClicked( MenuItem item ) {
    	if( isRunning ) {
    		item.setIcon( runIcon );
    	} else {
    		item.setIcon( pauseIcon );
    	}
    	Dreamcast.toggleRun();
    	isRunning = !isRunning;
    }
    
    public void onResetClicked( MenuItem item ) {
    	Dreamcast.reset();
    }
    
    public void onPreferencesClicked( MenuItem item ) {
    	/* TODO */
    }
}
