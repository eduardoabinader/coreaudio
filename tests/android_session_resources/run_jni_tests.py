"""Build/run the resource JNI driver against JVM fixtures, without a device.

This verifies JNI signatures, leases and failure handling, not Android services.
"""
import argparse
from pathlib import Path
import platform
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', type=Path, required=True)
    parser.add_argument('--cxx', default='clang++')
    parser.add_argument('--javac', default='javac')
    parser.add_argument('--java', default='java')
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    host = 'darwin-x86_64' if platform.system() == 'Darwin' else 'linux-x86_64'
    headers = options.ndk / 'toolchains/llvm/prebuilt' / host / 'sysroot/usr/include'
    if not (headers / 'jni.h').is_file():
        parser.error('NDK JNI headers not found')
    with tempfile.TemporaryDirectory(prefix='coreaudio-resource-jni-') as folder:
        output = Path(folder)
        library = output / ('resources.dylib' if platform.system() == 'Darwin' else 'resources.so')
        subprocess.run([options.cxx, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-D__ANDROID__', '-I' + str(root / 'include'),
                        '-I' + str(root / 'src/platform'), '-idirafter', str(headers),
                        '-dynamiclib' if platform.system() == 'Darwin' else '-shared', '-fPIC',
                        str(root / 'tests/AndroidSessionResourceJniTests.cpp'),
                        str(root / 'src/platform/AndroidSessionResourceJni.cpp'),
                        str(root / 'src/platform/AudioSession.cpp'),
                        str(root / 'src/platform/OutputRoute.cpp'), '-o', str(library)], check=True)
        classes = output / 'classes'
        classes.mkdir()
        sources = sorted(str(path) for path in (root / 'tests/android_session_resources/java').rglob('*.java'))
        subprocess.run([options.javac, '-Xlint:all', '-Werror', '-d', str(classes), *sources], check=True)
        subprocess.run([options.java, '-Xcheck:jni', '--enable-native-access=ALL-UNNAMED', '-cp', str(classes),
                        'com.gigantelabs.tests.ResourceDriverHarness', str(library)], check=True)


if __name__ == '__main__':
    main()
