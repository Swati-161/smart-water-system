# app

A new Flutter project.

## Getting Started

This project is a starting point for a Flutter application.

A few resources to get you started if this is your first Flutter project:

- [Learn Flutter](https://docs.flutter.dev/get-started/learn-flutter)
- [Write your first Flutter app](https://docs.flutter.dev/get-started/codelab)
- [Flutter learning resources](https://docs.flutter.dev/reference/learning-resources)

For help getting started with Flutter development, view the
[online documentation](https://docs.flutter.dev/), which offers tutorials,
samples, guidance on mobile development, and a full API reference.


## Firebase Setup (required before running)

This project uses Firebase. The config files are not in the repo for security.

### Steps for teammates:

1. Ask the project owner to add your Google account to the Firebase project
2. Install FlutterFire CLI:
dart pub global activate flutterfire_cli
3. Run inside the app/ folder:
flutterfire configure
    Select the `smAart-water-system` project when prompted.
   This generates `lib/firebase_options.dart` and `android/app/google-services.json` automatically.
4. Run `flutter pub get` then `flutter run`