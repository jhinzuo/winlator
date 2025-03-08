package com.winlator.widget;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Build;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.RequiresApi;

import com.winlator.R;

import com.winlator.core.FileUtils;
import com.winlator.core.GPUInformation;
import com.winlator.core.StringUtils;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Locale;
import org.json.JSONException;
import org.json.JSONObject;

public class FrameRating extends FrameLayout implements Runnable {
    private Context context;
    private long lastTime = 0;
    private int frameCount = 0;
    private File appInfo = null;
    private float lastFPS = 0;
    private String renderer = null;
    private String gpuName = null;
    private String totalRAM = null;
    private final TextView tvFPS;
    private final TextView tvRenderer;
    private final TextView tvCPU;
    private final TextView tvGPU;
    private final TextView tvRAM;

    public FrameRating(Context context) {
        this(context, null);
    }

    public FrameRating(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public FrameRating(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        this.context = context;
        View view = LayoutInflater.from(context).inflate(R.layout.frame_rating, this, false);
        tvFPS = view.findViewById(R.id.TVFPS);
        tvRenderer = view.findViewById(R.id.TVRenderer);
        tvCPU = view.findViewById(R.id.TVCPU);
        tvCPU.setText(getSOCName(context));
        tvGPU = view.findViewById(R.id.TVGPU);
        tvRAM = view.findViewById(R.id.TVRAM);
        totalRAM = getTotalRAM();
        appInfo = new File(context.getFilesDir(), "imagefs/tmp/app_info.txt");
        if (appInfo.exists()) appInfo.delete();
        addView(view);
    }
    
    private String getRenderer() {
        String renderer = "Unknown";
        ArrayList<String> lines = FileUtils.readLines(appInfo);
        if (lines.size() > 0)
            renderer = lines.get(0);
        return renderer;
    }
    
    private String getGPUName() {
        String gpuName = GPUInformation.getRenderer(context);
        ArrayList<String> lines = FileUtils.readLines(appInfo);
        if (lines.size() > 1) {
            gpuName = lines.get(1);
        }
        return gpuName;
    }

    private String getBoardName() {
        String boardName = "";
        try {
            boardName = (String)Class.forName("android.os.SystemProperties").getMethod("get", String.class).invoke(null, "ro.board.platform");
        }
        catch (Exception e) {
            Log.d("FrameRating", "Couldn't query board name, setting to generic board");
            boardName = "generic";
        }
        return boardName;
    }

    private String getSOCName(Context context) {
        String socName = "";
        InputStream is = null;

        try {
            is = context.getAssets().open("cpu_database.json");
            if (is != null) {
                BufferedReader br = new BufferedReader(new InputStreamReader(is));
                String line;
                StringBuilder sb = new StringBuilder();
                while ((line = br.readLine()) != null) {
                    sb.append(line);
                }
                String jsonString = new String(sb.toString());
                JSONObject jobj = new JSONObject(jsonString);
                JSONObject board = (JSONObject)jobj.get(getBoardName());
                socName = board.getString("SoC");
            }
        }
        catch (IOException | JSONException e) {
            Log.d("FrameRating", "Couldn't query SoC, defaulting to generic SoC");
            socName = "Generic Android AARCH64 CPU";
        }
        
        return socName;
    }
    
    private String getTotalRAM() {
        String totalRAM = "";
        ActivityManager activityManager = (ActivityManager)context.getSystemService(Context.ACTIVITY_SERVICE);
        ActivityManager.MemoryInfo memoryInfo = new ActivityManager.MemoryInfo();
        activityManager.getMemoryInfo(memoryInfo);
        totalRAM = StringUtils.formatBytes(memoryInfo.totalMem);
        return totalRAM;
    }
    
    private String getAvailableRAM() {
        String availableRAM = "";
        ActivityManager activityManager = (ActivityManager)context.getSystemService(Context.ACTIVITY_SERVICE);
        ActivityManager.MemoryInfo memoryInfo = new ActivityManager.MemoryInfo();
        activityManager.getMemoryInfo(memoryInfo);
        long usedMem = memoryInfo.totalMem - memoryInfo.availMem;
        availableRAM = StringUtils.formatBytes(usedMem, false);
        return availableRAM;
    }
    
    public void reset() {
        Log.d("FrameRating", "Resetting FrameRating");
        renderer = null;
        gpuName = null;
        lastFPS = 0;
        appInfo.delete();
    }

    public void update() {
        if (lastTime == 0) lastTime = SystemClock.elapsedRealtime();
        long time = SystemClock.elapsedRealtime();
        if (time >= lastTime + 500) {
            lastFPS = ((float)(frameCount * 1000) / (time - lastTime));
            post(this);
            lastTime = time;
            frameCount = 0;
        }
        frameCount++;
    }

    @Override
    public void run() {
        if (getVisibility() == GONE) setVisibility(View.VISIBLE);
        tvFPS.setText(String.format(Locale.ENGLISH, "%.1f", lastFPS));
        if (appInfo.exists() && renderer == null) {
            renderer = getRenderer();
            tvRenderer.setText(renderer);
        }
        if (appInfo.exists() && gpuName == null) {
            gpuName = getGPUName();
            tvGPU.setText(gpuName);
        }
        tvRAM.setText(getAvailableRAM() + " GB Used / " + totalRAM + " Total");
    }
}