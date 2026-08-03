package com.example.throttle_dashboard

import android.os.Bundle
import android.view.WindowManager
import io.flutter.embedding.android.FlutterActivity

class MainActivity : FlutterActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Display waehrend der Fahrt anlassen: sonst sperrt das Handy mitten in
        // der Aufnahme und MIUI raeumt die App im Hintergrund ab.
        // FLAG_KEEP_SCREEN_ON braucht keine Permission und gilt nur, solange
        // diese Activity im Vordergrund ist -> wird beim Verlassen automatisch
        // wieder freigegeben, kein Akku-Leck.
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }
}
