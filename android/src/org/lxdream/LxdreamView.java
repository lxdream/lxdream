/**
 * $Id$
 * 
 * Lxdream GL view. Derived from android  
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
/*
 * Copyright (C) 2008,2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


import android.content.Context;
import android.graphics.PixelFormat;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

class LxdreamView extends SurfaceView implements SurfaceHolder.Callback {
    private static String TAG = "LxdreamView";
    private static final boolean DEBUG = false;

    public LxdreamView(Context context) {
        super(context);
        getHolder().addCallback(this);
    }

    @Override
    public void surfaceCreated( SurfaceHolder holder ) {
    	/* Ignore */
    }
    
    @Override
    public void surfaceChanged( SurfaceHolder holder, int format, int width, int height ) {
    	setSurface( holder.getSurface(), width, height );
    }
    
    @Override
    public void surfaceDestroyed( SurfaceHolder holder ) {
    	clearSurface( holder.getSurface() );
    }
    
    private native void setSurface( Surface surface, int width, int height );
    private native void clearSurface( Surface surface );
    
}
