// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Reading.h"

#include <QImage>
#include <QStringList>

#include <memory>
#include <vector>

namespace Ort {
struct Env;
struct Session;
}

namespace Gooseberry {

// Reads handwriting with Microsoft's TrOCR handwriting model, run by ONNX
// Runtime on the computer (docs/DECISIONS.md § Handwriting is read by a small
// model on the computer). Each ruled line is drawn as a picture, black on
// white, and read; the reader's next-best readings give the runner-up words.
// The model is the folder install.sh fetches: encoder_model and decoder_model
// in ONNX, quantized or not, with tokenizer.json and generation_config.json.
class TrOcrReader : public InkReader
{
public:
    explicit TrOcrReader(const QString &folder);
    ~TrOcrReader() override;

    // Where the model is: GOOSEBERRY_READER_DIR, for tests and trials, or
    // gooseberry/reader in the system's data folders.
    static QString defaultFolder();
    // True when the folder holds a model this can load.
    static bool installedAt(const QString &folder);
    // The line as the model is shown it, before it is fitted to its size.
    static QImage picture(const QList<InkStroke> &line);

    std::optional<Read> read(const QList<InkStroke> &line) override;
    void rest() override;

private:
    struct Beam {
        std::vector<long long> ids;
        float score = 0;
        bool done = false;
    };

    bool load();
    std::vector<float> encode(const QImage &picture, std::vector<long long> &shape);
    std::vector<float> nextScores(const std::vector<float> &state, const std::vector<long long> &stateShape,
                                  const std::vector<long long> &ids);
    QString text(const std::vector<long long> &ids) const;

    QString m_folder;
    std::unique_ptr<Ort::Env> m_env;
    std::unique_ptr<Ort::Session> m_encoder;
    std::unique_ptr<Ort::Session> m_decoder;
    QStringList m_pieces;
    QList<bool> m_special;
    bool m_byteLevel = false;
    long long m_start = 2;
    long long m_end = 2;
    int m_longest = 40;
};

} // namespace Gooseberry
