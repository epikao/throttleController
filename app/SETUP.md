# throttle_dashboard — Setup

## Voraussetzungen
- Flutter SDK installiert: https://docs.flutter.dev/get-started/install
- Windows: Visual Studio Build Tools (für Windows-Desktop-Target)
- Android: Android SDK + USB-Debugging am Handy aktiviert

## Projekt erstellen

```bash
cd i:/Pierre/Technical/throttlecontrol/software
flutter create app --project-name throttle_dashboard --platforms=windows,android
```

Danach die generierten Dateien durch die vorhandenen ersetzen:
- `app/pubspec.yaml` → bereits vorhanden
- `app/lib/` → bereits vorhanden

```bash
cd app
flutter pub get
```

## Android: USB-Berechtigung

In `android/app/src/main/AndroidManifest.xml` innerhalb von `<manifest>` ergänzen:

```xml
<uses-feature android:name="android.hardware.usb.host" />
```

Und innerhalb von `<application>`:

```xml
<activity ...>
    ...
    <intent-filter>
        <action android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED" />
    </intent-filter>
    <meta-data
        android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED"
        android:resource="@xml/device_filter" />
</activity>
```

Datei `android/app/src/main/res/xml/device_filter.xml` anlegen:

```xml
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <usb-device />
</resources>
```

## Bauen & Starten

**Windows:**
```bash
flutter run -d windows
```

**Android (USB-Debugging):**
```bash
flutter run -d <device-id>
```

## Bedienung

1. App starten → Port aus Liste wählen → "Verbinden"
2. App sendet `?` → Firmware wechselt automatisch in JSON-Modus
3. Dashboard zeigt alle Werte live (200ms Update)
4. "Trennen"-Button → Firmware kehrt zu ANSI/TeraTerm-Modus zurück (`!`)

## JSON-Protokoll (Firmware → App)

```
{"sp":23.4,"cd":0.82,"tq":512,"pw":350,"cu":8.5,"vo":48.2,
 "th":2.34,"tp":24.8,"tr":12.3,"od":1234,"ra":45,"wh":18.5,
 "ca":87,"sa":22.1,"ot":3600,"t":1800,
 "rc":0,"su":8,"sw":12345,"ss":1,"crs":0,"pg":0}
```

| Key | Bedeutung        | Typ   |
|-----|-----------------|-------|
| sp  | Speed km/h      | float |
| cd  | Cadence 0-1     | float |
| tq  | Torque ADC      | int   |
| pw  | Power W         | int   |
| cu  | Current A       | float |
| vo  | Voltage V       | float |
| th  | Throttle V      | float |
| tp  | Temp °C         | float |
| tr  | Trip km         | float |
| od  | Odo km          | int   |
| ra  | Range km        | int   |
| wh  | Wh/km           | float |
| ca  | Capacity %      | int   |
| sa  | SpeedAvg km/h   | float |
| ot  | OnTime sec      | int   |
| t   | RideTime sec    | int   |
| rc  | Race mode 0/1   | int   |
| su  | Support level   | int   |
| sw  | Stopwatch cs    | int   |
| ss  | SW state 0-2    | int   |
| crs | Cruise btn 0/1  | int   |
| pg  | Menu page       | int   |
