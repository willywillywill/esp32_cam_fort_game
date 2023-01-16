package com.example.esp32_cam_game_for_app;

import androidx.appcompat.app.AppCompatActivity;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;


public class MainActivity extends AppCompatActivity {

    String url = "http://192.168.1.160";

    Button top, bottom, left, right, other, attack;
    int attacked_number;
    TextView text_1;
    ImageView camera_1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getSupportActionBar().hide();
        setContentView(R.layout.activity_main);

        top = findViewById(R.id.top);
        bottom = findViewById(R.id.bottom);
        left = findViewById(R.id.left);
        right = findViewById(R.id.right);
        other = findViewById(R.id.other);
        attack = findViewById(R.id.attack);

        text_1 = findViewById(R.id.text_1);
        camera_1 = findViewById(R.id.camera_view);

        new Thread(() -> {
            VideoStream();
        }).start();
        new Thread(() -> {
            //getlight();
        }).start();


        set_req_btn_click(top,"top");
        set_req_btn_click(bottom,"bottom");
        set_req_btn_long(left,"left");
        set_req_btn_long(right,"right");
        set_req_btn_long(attack,"attack");





    }

    private void getlight()
    {
        String rssi_url = url + "/light";

        try {
            URL url = new URL(rssi_url);

            try {

                HttpURLConnection huc = (HttpURLConnection) url.openConnection();
                huc.setRequestMethod("GET");
                huc.setConnectTimeout(100 * 5);
                huc.setReadTimeout(100 * 5);
                huc.setDoInput(true);
                huc.connect();
                if (huc.getResponseCode() == 200) {
                    InputStream in = huc.getInputStream();

                    InputStreamReader isr = new InputStreamReader(in);
                    BufferedReader br = new BufferedReader(isr);
                    final String data = br.readLine();
                    if (!data.isEmpty()) {
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                //text_1.setText(data);
                                other.setText(data);
                            }
                        });
                    }

                }

            } catch (Exception e) {
                e.printStackTrace();
            }
        }catch (MalformedURLException e) {
            e.printStackTrace();
        }
    }



    private void VideoStream()
    {
        String stream_url = url + ":81/camera";

        BufferedInputStream bis = null;
        FileOutputStream fos = null;
        try
        {

            URL url = new URL(stream_url);

            try
            {
                HttpURLConnection huc = (HttpURLConnection) url.openConnection();
                huc.setRequestMethod("GET");
                huc.setConnectTimeout(1000 * 5);
                huc.setReadTimeout(1000 * 5);
                huc.setDoInput(true);
                huc.connect();

                if (huc.getResponseCode() == 200)
                {
                    InputStream in = huc.getInputStream();

                    InputStreamReader isr = new InputStreamReader(in);
                    BufferedReader br = new BufferedReader(isr);

                    String data;

                    int len;
                    byte[] buffer;

                    while ((data = br.readLine()) != null)
                    {
                        if (data.contains("Content-Type:"))
                        {
                            data = br.readLine();

                            len = Integer.parseInt(data.split(":")[1].trim());

                            bis = new BufferedInputStream(in);
                            buffer = new byte[len];

                            int t = 0;
                            while (t < len)
                            {
                                t += bis.read(buffer, t, len - t);
                            }

                            Bytes2ImageFile(buffer, getExternalFilesDir(Environment.DIRECTORY_PICTURES) + "/0A.jpg");

                            final Bitmap bitmap = BitmapFactory.decodeFile(getExternalFilesDir(Environment.DIRECTORY_PICTURES) + "/0A.jpg");

                            runOnUiThread(new Runnable()
                            {
                                @Override
                                public void run()
                                {
                                    camera_1.setImageBitmap(bitmap);
                                }
                            });

                        }
                    }
                }

            } catch (IOException e)
            {
                e.printStackTrace();
            }
        } catch (MalformedURLException e)
        {
            e.printStackTrace();
        } finally
        {
            try
            {
                if (bis != null)
                {
                    bis.close();
                }
                if (fos != null)
                {
                    fos.close();
                }
            } catch (IOException e)
            {
                e.printStackTrace();
            }
        }

    }

    private void Bytes2ImageFile(byte[] bytes, String fileName)
    {
        try
        {
            File file = new File(fileName);
            FileOutputStream fos = new FileOutputStream(file);
            fos.write(bytes, 0, bytes.length);
            fos.flush();
            fos.close();
        } catch (Exception e)
        {
            e.printStackTrace();
        }
    }



    void set_req_btn_click(Button btn, String cmd){
        btn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                new Thread(() ->{
                    try{
                        Post_data(cmd);
                    }catch (Exception e){
                        System.out.println(e);
                    }
                }).start();
            }
        });
    }

    void set_req_btn_long(Button btn, String cmd){

        final boolean[] long_click = {false};

        btn.setOnLongClickListener(new View.OnLongClickListener() {
            @Override
            public boolean onLongClick(View v) {
                new Thread(() ->{
                    try {
                        long_click[0] = true;
                        Post_data(cmd+"T");
                    }catch (Exception e){
                        long_click[0] = true;
                    }

                }).start();

                return true;
            }

        });

        btn.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                new Thread(() -> {
                    if (event.getAction() == MotionEvent.ACTION_UP) {
                        if (long_click[0]) {
                            try {
                                Post_data(cmd+"F");
                                long_click[0] = false;
                            }catch (Exception e) {
                                long_click[0] = false;
                            }
                        }
                    }

                }).start();
                return false;
            }
        });

    }
    void Post_data(String cmd){
        try {
            String json = "cmd="+cmd;

            HttpURLConnection con = (HttpURLConnection) new URL(url+":80/command").openConnection();
            con.setRequestMethod("POST");
            con.setConnectTimeout(5000);
            con.setReadTimeout(5000);
            con.setRequestProperty("Accept", "application/json");
            con.setDoOutput(true);

            try (DataOutputStream wr = new DataOutputStream(con.getOutputStream())) {
                wr.writeBytes(json);
                wr.flush();
            }
            con.getResponseCode();

        } catch (IOException e) {
            e.printStackTrace();
        }

    }

}