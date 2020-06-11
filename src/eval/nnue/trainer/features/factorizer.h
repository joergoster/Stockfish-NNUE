// NNUE評価関数の特徴量変換クラステンプレート
// Feature Transformation Class for NNUE Evaluation Function

#ifndef _NNUE_TRAINER_FEATURES_FACTORIZER_H_
#define _NNUE_TRAINER_FEATURES_FACTORIZER_H_

#if defined(EVAL_NNUE)

#include "../../nnue_common.h"
#include "../trainer.h"

namespace Eval {
  namespace NNUE {
    namespace Features {

      // 入力特徴量を学習用特徴量に変換するクラステンプレート
      // デフォルトでは学習用特徴量は元の入力特徴量と同じとし、必要に応じて特殊化する
      // Template class for converting input features into training features
      // By default, the training features are the same as the original input features,
      // and can be specialized if necessary.
      template <typename FeatureType>
      class Factorizer {
      public:
        // 学習用特徴量の次元数を取得する
        // Get the number of dimensions of the training features
        static constexpr IndexType GetDimensions() {
          return FeatureType::kDimensions;
        }

        // 学習用特徴量のインデックスと学習率のスケールを取得する
        // Get the index of training features and the learning rate scale
        static void AppendTrainingFeatures(IndexType base_index, std::vector<TrainingFeature>* training_features) {
          assert(base_index < FeatureType::kDimensions);

          training_features->emplace_back(base_index);
        }
      };

      // 学習用特徴量の情報
      // Information on training features
      struct FeatureProperties {
        bool active;
        IndexType dimensions;
      };

      // 元の入力特徴量を学習用特徴量に追加する
      // Add the original input features to the training features
      template <typename FeatureType>
      IndexType AppendBaseFeature(FeatureProperties properties, IndexType base_index,
                                  std::vector<TrainingFeature>* training_features) {

        assert(properties.dimensions == FeatureType::kDimensions);
        assert(base_index < FeatureType::kDimensions);

        training_features->emplace_back(base_index);

        return properties.dimensions;
      }

      // 学習率のスケールが0でなければ他の種類の学習用特徴量を引き継ぐ
      // If the learning rate scale is non-zero, it inherits other types of training features.
      template <typename FeatureType>
      IndexType InheritFeaturesIfRequired(IndexType index_offset, FeatureProperties properties, IndexType base_index,
                                          std::vector<TrainingFeature>* training_features) {
        if (!properties.active)
            return 0;

        assert(properties.dimensions == Factorizer<FeatureType>::GetDimensions());
        assert(base_index < FeatureType::kDimensions);

        const auto start = training_features->size();
        Factorizer<FeatureType>::AppendTrainingFeatures(base_index, training_features);

        for (auto i = start; i < training_features->size(); ++i)
        {
            auto& feature = (*training_features)[i];
            assert(feature.GetIndex() < Factorizer<FeatureType>::GetDimensions());

            feature.ShiftIndex(index_offset);
        }

        return properties.dimensions;
      }

      // 学習用特徴量を追加せず、必要に応じてインデックスの差分を返す
      // 対応する特徴量がない場合にInheritFeaturesIfRequired()の代わりに呼ぶ
      // Do not add training features, but return index differences as needed.
      // Call this instead of InheritFeaturesIfRequired() when the corresponding feature is missing.
      IndexType SkipFeatures(FeatureProperties properties) {
        return properties.active ? properties.dimensions : 0;
      }

      // 学習用特徴量の次元数を取得する
      // Get the number of dimensions of the training features
      template <std::size_t N>
      constexpr IndexType GetActiveDimensions(const FeatureProperties (&properties)[N]) {

        static_assert(N > 0, "");

        IndexType dimensions = properties[0].dimensions;

        for (std::size_t i = 1; i < N; ++i)
            if (properties[i].active)
                dimensions += properties[i].dimensions;

        return dimensions;
      }

      // 配列の要素数を取得する
      // Get the number of array elements
      template <typename T, std::size_t N>
      constexpr std::size_t GetArrayLength(const T (&/*array*/)[N]) {
        return N;
      }

    } // namespace Features
  } // namespace NNUE
} // namespace Eval

#endif  // defined(EVAL_NNUE)

#endif
