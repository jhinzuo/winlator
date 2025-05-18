package com.winlator.core;

import android.content.Context;
import android.content.SharedPreferences;
import android.opengl.EGL14;

import androidx.collection.ArrayMap;
import androidx.preference.PreferenceManager;

import java.util.Locale;
import java.util.Objects;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.opengles.GL10;

public abstract class GPUInformation {

    public static boolean isAdreno6xx() {
        return getRenderer().toLowerCase(Locale.ENGLISH).matches(".*adreno[^6]+6[0-9]{2}.*");
    }

    public static boolean isAdreno7xx() {
        return getRenderer().toLowerCase(Locale.ENGLISH).matches(".*adreno[^7]+7[0-9]{2}.*");
    }

    public static boolean isAdreno8xx() {
        return getRenderer().toLowerCase(Locale.ENGLISH).matches(".*adreno[^8]+8[0-9]{2}.*");
    }

    public native static String getVersion();
    public native static String getRenderer();
    public native static long getMemorySize();
    public native static String[] enumerateExtensions();

    static {
        System.loadLibrary("winlator");
    }
}
