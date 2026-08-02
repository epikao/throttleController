import 'package:flutter/material.dart';
import 'connection_screen.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const ThrottleDashApp());
}

class ThrottleDashApp extends StatelessWidget {
  const ThrottleDashApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'eBike Dashboard',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: Colors.cyan,
          brightness: Brightness.dark,
        ),
        useMaterial3: true,
        fontFamily: 'monospace',
      ),
      home: const ConnectionScreen(),
    );
  }
}
