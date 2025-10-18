# Google Speech Commands Dataset (v0.02)

This repository does not include the full Google Speech Commands dataset due to its large size.

Official download links and instructions:

- Google AI Blog announcement (info): https://ai.googleblog.com/2017/08/launching-speech-commands-dataset.html
- Direct download (v0.02) from TensorFlow dataset archive:
  - https://storage.googleapis.com/download.tensorflow.org/data/speech_commands_v0.02.tar.gz

Quick steps to download and prepare the dataset:

1. Download the archive (recommended on a machine with enough storage):

```bash
curl -o speech_commands_v0.02.tar.gz https://storage.googleapis.com/download.tensorflow.org/data/speech_commands_v0.02.tar.gz
```

2. Extract into the repository `dataset/` folder (do NOT commit the extracted files):

```bash
mkdir -p dataset
tar -xzf speech_commands_v0.02.tar.gz -C dataset
```

3. Verify the structure (each keyword is a directory):

```
dataset/
├── _background_noise_/
├── yes/
├── no/
├── up/
└── ...
```

4. IMPORTANT: Do not add the `dataset/` folder to Git. It's already listed in `.gitignore`.

If you prefer, you can download directly in Python using the training script which will attempt to fetch the dataset when `DATA_URL` is set.

---

If you want, I can add an automated small script `scripts/download_dataset.sh` or a Windows batch `scripts/download_dataset.bat` to download and extract the dataset for you.