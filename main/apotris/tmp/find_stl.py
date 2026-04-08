import os
ndk_root = r'C:\Users\sanay\AppData\Local\Android\Sdk\ndk\26.1.10909125'
for root, dirs, files in os.walk(ndk_root):
    for file in files:
        if file == 'libc++_shared.so':
            if 'aarch64' in root or 'arm64-v8a' in root:
                print(os.path.join(root, file))
