// SPDX-License-Identifier: GPL-2.0-or-later
#include "TrOcrReader.h"

#include <onnxruntime_cxx_api.h>

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>
#include <QDebug>

#include <algorithm>
#include <cmath>

namespace Gooseberry {

namespace {

// The model's picture: 384 pixels square, as it was trained.
constexpr int Side = 384;
// How many readings are carried along at once; the best is the reading, the
// others give the runner-up words.
constexpr int Beams = 3;
// Pixels per page unit when a line is drawn for reading.
constexpr qreal DrawScale = 2;
// Room round the writing.
constexpr qreal Margin = 10;

QString firstOf(const QString &folder, const QStringList &names)
{
    for (const QString &name : names) {
        const QString path = folder + QLatin1Char('/') + name;
        if (QFile::exists(path)) {
            return path;
        }
    }
    return {};
}

QString encoderPath(const QString &folder)
{
    return firstOf(folder, {QStringLiteral("encoder_model_quantized.onnx"), QStringLiteral("encoder_model.onnx")});
}

QString decoderPath(const QString &folder)
{
    return firstOf(folder, {QStringLiteral("decoder_model_quantized.onnx"), QStringLiteral("decoder_model.onnx")});
}

QJsonObject jsonFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

// Byte-level pieces stand for bytes by printable characters; this undoes it.
QHash<QChar, char> byteDecoder()
{
    QList<int> bytes;
    for (int b = '!'; b <= '~'; ++b) {
        bytes.append(b);
    }
    for (int b = 0xA1; b <= 0xAC; ++b) {
        bytes.append(b);
    }
    for (int b = 0xAE; b <= 0xFF; ++b) {
        bytes.append(b);
    }
    QList<int> chars = bytes;
    int extra = 0;
    for (int b = 0; b < 256; ++b) {
        if (!bytes.contains(b)) {
            bytes.append(b);
            chars.append(256 + extra++);
        }
    }
    QHash<QChar, char> decoder;
    for (qsizetype i = 0; i < bytes.size(); ++i) {
        decoder.insert(QChar(chars.at(i)), char(bytes.at(i)));
    }
    return decoder;
}

std::vector<std::string> names(Ort::Session &session, bool inputs)
{
    Ort::AllocatorWithDefaultOptions allocator;
    std::vector<std::string> found;
    const size_t count = inputs ? session.GetInputCount() : session.GetOutputCount();
    for (size_t i = 0; i < count; ++i) {
        found.push_back(inputs ? session.GetInputNameAllocated(i, allocator).get()
                               : session.GetOutputNameAllocated(i, allocator).get());
    }
    return found;
}

std::vector<const char *> pointers(const std::vector<std::string> &strings)
{
    std::vector<const char *> out;
    for (const auto &s : strings) {
        out.push_back(s.c_str());
    }
    return out;
}

} // namespace

TrOcrReader::TrOcrReader(const QString &folder)
    : m_folder(folder)
{
}

TrOcrReader::~TrOcrReader() = default;

QString TrOcrReader::defaultFolder()
{
    const QString chosen = qEnvironmentVariable("GOOSEBERRY_READER_DIR");
    if (!chosen.isEmpty()) {
        return chosen;
    }
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("gooseberry/reader"),
                                  QStandardPaths::LocateDirectory);
}

bool TrOcrReader::installedAt(const QString &folder)
{
    return !folder.isEmpty() && !encoderPath(folder).isEmpty() && !decoderPath(folder).isEmpty()
        && QFile::exists(folder + QStringLiteral("/tokenizer.json"));
}

QImage TrOcrReader::picture(const QList<InkStroke> &line)
{
    QRectF bounds;
    for (const InkStroke &stroke : line) {
        for (const QPointF &point : stroke.outline()) {
            bounds = bounds.isNull() ? QRectF(point, QSizeF(0.01, 0.01)) : bounds.united(QRectF(point, QSizeF(0.01, 0.01)));
        }
    }
    bounds.adjust(-Margin, -Margin, Margin, Margin);
    QImage image(QSize(int(std::ceil(bounds.width() * DrawScale)), int(std::ceil(bounds.height() * DrawScale))),
                 QImage::Format_RGB888);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(DrawScale, DrawScale);
    painter.translate(-bounds.topLeft());
    painter.setPen(Qt::NoPen);
    // Every ink is read as black on white, as the model learned to read.
    painter.setBrush(Qt::black);
    for (const InkStroke &stroke : line) {
        painter.drawPolygon(QPolygonF(stroke.outline()));
    }
    return image;
}

bool TrOcrReader::load()
{
    if (m_encoder && m_decoder) {
        return true;
    }
    if (!installedAt(m_folder)) {
        return false;
    }
    try {
        if (!m_env) {
            m_env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_ERROR, "gooseberry");
        }
        Ort::SessionOptions options;
        // Two threads at most: reading stays out of the way of everything else.
        options.SetIntraOpNumThreads(2);
        options.SetInterOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        m_encoder = std::make_unique<Ort::Session>(*m_env, QFile::encodeName(encoderPath(m_folder)).constData(), options);
        m_decoder = std::make_unique<Ort::Session>(*m_env, QFile::encodeName(decoderPath(m_folder)).constData(), options);
    } catch (const Ort::Exception &error) {
        qWarning().noquote() << "The handwriting reader could not be loaded:" << error.what();
        m_encoder.reset();
        m_decoder.reset();
        return false;
    }

    const QJsonObject tokenizer = jsonFile(m_folder + QStringLiteral("/tokenizer.json"));
    const QJsonObject model = tokenizer.value(QStringLiteral("model")).toObject();
    m_pieces.clear();
    m_special.clear();
    if (model.value(QStringLiteral("vocab")).isArray()) {
        // Word pieces, in order of their numbers.
        const QJsonArray vocab = model.value(QStringLiteral("vocab")).toArray();
        for (const QJsonValue &entry : vocab) {
            m_pieces.append(entry.toArray().at(0).toString());
        }
    } else {
        // Pieces named with their numbers.
        const QJsonObject vocab = model.value(QStringLiteral("vocab")).toObject();
        int largest = -1;
        for (auto it = vocab.begin(); it != vocab.end(); ++it) {
            largest = std::max(largest, it.value().toInt());
        }
        m_pieces.resize(largest + 1);
        for (auto it = vocab.begin(); it != vocab.end(); ++it) {
            m_pieces[it.value().toInt()] = it.key();
        }
        m_byteLevel = model.value(QStringLiteral("type")).toString() == QLatin1String("BPE");
    }
    m_special = QList<bool>(m_pieces.size(), false);
    const QJsonArray added = tokenizer.value(QStringLiteral("added_tokens")).toArray();
    for (const QJsonValue &value : added) {
        const QJsonObject token = value.toObject();
        const int id = token.value(QStringLiteral("id")).toInt(-1);
        if (id >= m_pieces.size()) {
            m_pieces.resize(id + 1);
            m_special.resize(id + 1);
        }
        if (id >= 0) {
            m_pieces[id] = token.value(QStringLiteral("content")).toString();
            m_special[id] = token.value(QStringLiteral("special")).toBool();
        }
    }
    QJsonObject generation = jsonFile(m_folder + QStringLiteral("/generation_config.json"));
    if (generation.isEmpty()) {
        generation = jsonFile(m_folder + QStringLiteral("/config.json"));
    }
    m_start = generation.value(QStringLiteral("decoder_start_token_id")).toInteger(2);
    m_end = generation.value(QStringLiteral("eos_token_id")).toInteger(2);
    m_longest = std::clamp(generation.value(QStringLiteral("max_length")).toInt(40), 8, 64);
    return !m_pieces.isEmpty();
}

void TrOcrReader::rest()
{
    m_encoder.reset();
    m_decoder.reset();
}

std::vector<float> TrOcrReader::encode(const QImage &picture, std::vector<long long> &shape)
{
    const QImage fitted = picture.scaled(Side, Side, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                              .convertToFormat(QImage::Format_RGB888);
    std::vector<float> pixels(size_t(3 * Side * Side));
    for (int y = 0; y < Side; ++y) {
        const uchar *line = fitted.constScanLine(y);
        for (int x = 0; x < Side; ++x) {
            for (int c = 0; c < 3; ++c) {
                // As the model was trained: each colour from -1 to 1.
                pixels[size_t(c * Side * Side + y * Side + x)] = (line[x * 3 + c] / 255.0f - 0.5f) / 0.5f;
            }
        }
    }
    const std::vector<long long> inputShape = {1, 3, Side, Side};
    const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input = Ort::Value::CreateTensor<float>(memory, pixels.data(), pixels.size(), reinterpret_cast<const int64_t *>(inputShape.data()),
                                                       inputShape.size());
    const auto inputs = names(*m_encoder, true);
    const auto outputs = names(*m_encoder, false);
    const auto inNames = pointers(inputs);
    const auto outNames = pointers(outputs);
    auto results = m_encoder->Run(Ort::RunOptions{nullptr}, inNames.data(), &input, 1, outNames.data(), 1);
    const auto info = results.front().GetTensorTypeAndShapeInfo();
    const auto dims = info.GetShape();
    shape.assign(dims.begin(), dims.end());
    const float *data = results.front().GetTensorData<float>();
    return std::vector<float>(data, data + info.GetElementCount());
}

std::vector<float> TrOcrReader::nextScores(const std::vector<float> &state, const std::vector<long long> &stateShape,
                                           const std::vector<long long> &ids)
{
    const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::vector<int64_t> idValues(ids.begin(), ids.end());
    const std::vector<int64_t> idShape = {1, int64_t(ids.size())};
    std::vector<float> stateCopy = state;
    std::vector<int64_t> stateDims(stateShape.begin(), stateShape.end());
    std::vector<int64_t> mask(size_t(stateDims.size() > 1 ? stateDims[1] : 1), 1);
    const std::vector<int64_t> maskShape = {1, int64_t(mask.size())};

    const auto inputs = names(*m_decoder, true);
    std::vector<Ort::Value> values;
    for (const std::string &name : inputs) {
        if (name.find("input_ids") != std::string::npos) {
            values.push_back(Ort::Value::CreateTensor<int64_t>(memory, idValues.data(), idValues.size(), idShape.data(), idShape.size()));
        } else if (name.find("attention_mask") != std::string::npos) {
            values.push_back(Ort::Value::CreateTensor<int64_t>(memory, mask.data(), mask.size(), maskShape.data(), maskShape.size()));
        } else {
            values.push_back(Ort::Value::CreateTensor<float>(memory, stateCopy.data(), stateCopy.size(), stateDims.data(), stateDims.size()));
        }
    }
    const auto outputs = names(*m_decoder, false);
    const auto inNames = pointers(inputs);
    const char *logitsName = outputs.front().c_str();
    auto results = m_decoder->Run(Ort::RunOptions{nullptr}, inNames.data(), values.data(), values.size(), &logitsName, 1);
    const auto dims = results.front().GetTensorTypeAndShapeInfo().GetShape();
    const size_t vocabulary = size_t(dims.back());
    const float *logits = results.front().GetTensorData<float>() + (ids.size() - 1) * vocabulary;
    // The chances of each next piece, as logarithms.
    const float largest = *std::max_element(logits, logits + vocabulary);
    double sum = 0;
    for (size_t i = 0; i < vocabulary; ++i) {
        sum += std::exp(double(logits[i] - largest));
    }
    const float base = largest + float(std::log(sum));
    std::vector<float> scores(vocabulary);
    for (size_t i = 0; i < vocabulary; ++i) {
        scores[i] = logits[i] - base;
    }
    return scores;
}

QString TrOcrReader::text(const std::vector<long long> &ids) const
{
    QString joined;
    for (const long long id : ids) {
        if (id < 0 || id >= m_pieces.size() || id == m_start || id == m_end || m_special.value(int(id))) {
            continue;
        }
        joined += m_pieces.at(int(id));
    }
    if (m_byteLevel) {
        static const QHash<QChar, char> decoder = byteDecoder();
        QByteArray bytes;
        for (const QChar c : std::as_const(joined)) {
            bytes.append(decoder.value(c, '?'));
        }
        joined = QString::fromUtf8(bytes);
    } else {
        joined.replace(QChar(0x2581), QLatin1Char(' '));
    }
    return joined.simplified();
}

std::optional<InkReader::Read> TrOcrReader::read(const QList<InkStroke> &line)
{
    if (line.isEmpty() || !load()) {
        return std::nullopt;
    }
    try {
        std::vector<long long> stateShape;
        const std::vector<float> state = encode(picture(line), stateShape);

        std::vector<Beam> beams = {{{m_start}, 0, false}};
        for (int step = 0; step < m_longest; ++step) {
            std::vector<Beam> next;
            for (const Beam &beam : beams) {
                if (beam.done) {
                    next.push_back(beam);
                    continue;
                }
                const std::vector<float> scores = nextScores(state, stateShape, beam.ids);
                std::vector<size_t> order(scores.size());
                for (size_t i = 0; i < order.size(); ++i) {
                    order[i] = i;
                }
                std::partial_sort(order.begin(), order.begin() + std::min<size_t>(Beams, order.size()), order.end(),
                                  [&scores](size_t a, size_t b) { return scores[a] > scores[b]; });
                for (size_t k = 0; k < std::min<size_t>(Beams, order.size()); ++k) {
                    Beam grown = beam;
                    grown.ids.push_back((long long)order[k]);
                    grown.score += scores[order[k]];
                    grown.done = (long long)order[k] == m_end;
                    next.push_back(grown);
                }
            }
            // The likeliest, each weighed for its length so a short reading
            // is not preferred for being short.
            auto weighed = [](const Beam &beam) {
                return beam.score / std::pow(float(beam.ids.size()), 0.6f);
            };
            std::sort(next.begin(), next.end(), [&weighed](const Beam &a, const Beam &b) { return weighed(a) > weighed(b); });
            if (next.size() > size_t(Beams)) {
                next.resize(Beams);
            }
            beams = std::move(next);
            if (std::all_of(beams.begin(), beams.end(), [](const Beam &beam) { return beam.done; })) {
                break;
            }
        }

        Read result;
        result.text = text(beams.front().ids);
        const QStringList read = result.text.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (size_t i = 1; i < beams.size(); ++i) {
            const QStringList words = text(beams[i].ids).split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString &word : words) {
                if (!read.contains(word.toLower()) && !result.also.contains(word)) {
                    result.also.append(word);
                }
            }
        }
        return result;
    } catch (const Ort::Exception &error) {
        qWarning().noquote() << "The handwriting reader could not read a line:" << error.what();
        return std::nullopt;
    }
}

} // namespace Gooseberry
