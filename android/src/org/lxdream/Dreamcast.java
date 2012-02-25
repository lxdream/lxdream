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

public class Dreamcast {

     static {
         System.loadLibrary("lxdream");
     }

     /* Core emulation */
     public static native void init();
     public static native void setViewSize(int width, int height);
     public static native void run();
     public static native void stop();
     /*
     public static native void start();
     public static native void run_slice();
     public static boolean canRun();
*/
     /* Save state management */
/*     public static native boolean saveState( String filename );
     public static native boolean loadState( String filename );
     public static native boolean quickSave();
     public static native boolean quickLoad();
     public static native void setQuickState(int state);
  */   
     /* GD-Rom */
/*     public static native boolean mount_disc( String filename );
     public static native void unmount_disc();
  */   
     /* ... */
}
