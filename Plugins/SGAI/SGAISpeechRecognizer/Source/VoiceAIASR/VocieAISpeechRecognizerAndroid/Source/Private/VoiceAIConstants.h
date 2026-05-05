// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

namespace VoiceAIConstants
{
constexpr int EVENT_SPEECH_STARTED = 100;
constexpr int EVENT_SPEECH_STOPPED = 101;
constexpr int ERROR_NO_INITIALIZED = 200;
constexpr int ERROR_PARAMS_INVALID = 201;
constexpr int ERROR_NO_SPEECH_TIMEOUT = 202;

constexpr const char *KEY__PATH_ADSP = "key_path_adsp";
constexpr const char *KEY__PATH_VAD_MODEL = "key_path_vad_model";
constexpr const char *KEY__MODEL_FILE_ENCODER = "key_model_file_encoder";
constexpr const char *KEY__MODEL_FILE_DECODER = "key_model_file_decoder";
constexpr const char *KEY__VOCAB_FILE = "key_vocab_file";
constexpr const char *ONE = "1";
constexpr const char *ZERO = "0";
constexpr const char *TRUE = "1";
constexpr const char *FALSE = "0";

constexpr const char *KEY__IS_FINAL = "is_final";
constexpr const char *KEY__TRANSCRIPTION = "transcription";
constexpr const char *KEY__CONFIGURATION_LANGUAGE = "language";
constexpr const char *KEY__CONFIGURATION_NOSTARTSPEECHTIMEOUT = "nostartspeechtimeout";
constexpr const char *KEY__CONFIGURATION_TRANSLATE = "translate";
constexpr const char *KEY__CONFIGURATION_CONTINUOUS = "continuous";

constexpr const char *SPEECH = "/speech_float.eai";
constexpr const char *ENCODER_FILE = "/encoder.bin";
constexpr const char *DECODER_FILE = "/decoder.bin";
constexpr const char *VOCAB = "/vocab.bin";
} // namespace VoiceAIConstants
