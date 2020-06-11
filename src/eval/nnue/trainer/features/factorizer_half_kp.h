// NNUE評価関数の特徴量変換クラステンプレートのHalfKP用特殊化
// Specialization of the feature conversion class template of the NNUE evaluation function for HalfKP

#ifndef _NNUE_TRAINER_FEATURES_FACTORIZER_HALF_KP_H_
#define _NNUE_TRAINER_FEATURES_FACTORIZER_HALF_KP_H_

#if defined(EVAL_NNUE)

#include "factorizer.h"
#include "../../features/half_kp.h"
#include "../../features/half_relative_kp.h"
#include "../../features/p.h"

namespace Eval {
  namespace NNUE {
    namespace Features {

      // 入力特徴量を学習用特徴量に変換するクラステンプレート
      // HalfKP用特殊化
      // Template class for converting input features into training features.
      // Specialization for HalfKP.
      template <Side AssociatedKing>
      class Factorizer<HalfKP<AssociatedKing>> {
      private:
        using FeatureType = HalfKP<AssociatedKing>;

        // 特徴量のうち、同時に値が1となるインデックスの数の最大値
        // The maximum number of indices that simultaneously have a value of 1 among the features
        static constexpr IndexType kMaxActiveDimensions = FeatureType::kMaxActiveDimensions;

        // 学習用特徴量の種類
        // Types of learning features
        enum TrainingFeatureType {
          kFeaturesHalfKP,
          kFeaturesHalfK,
          kFeaturesP,
          kFeaturesHalfRelativeKP,
          kNumTrainingFeatureTypes,
        };

        // 学習用特徴量の情報
        // Information on training features
        static constexpr FeatureProperties kProperties[] = {
          // kFeaturesHalfKP
          {true, FeatureType::kDimensions},
          // kFeaturesHalfK
          {true, SQUARE_NB},
          // kFeaturesP
          {true, Factorizer<P>::GetDimensions()},
          // kFeaturesHalfRelativeKP
          {true, Factorizer<HalfRelativeKP<AssociatedKing>>::GetDimensions()},
        };

        static_assert(GetArrayLength(kProperties) == kNumTrainingFeatureTypes, "");

      public:
        // 学習用特徴量の次元数を取得する
        // Get the number of dimensions of the training features
        static constexpr IndexType GetDimensions() {
          return GetActiveDimensions(kProperties);
        }

        // 学習用特徴量のインデックスと学習率のスケールを取得する
        // Get the index of training features and the learning rate scale
        static void AppendTrainingFeatures(IndexType base_index,
                                           std::vector<TrainingFeature>* training_features) {
          // kFeaturesHalfKP
          IndexType index_offset = AppendBaseFeature<FeatureType>(kProperties[kFeaturesHalfKP],
                                                                  base_index, training_features);

          const auto sq_k = static_cast<Square>(base_index / fe_end);
          const auto p = static_cast<BonaPiece>(base_index % fe_end);

          // kFeaturesHalfK
          const auto& properties = kProperties[kFeaturesHalfK];

          if (properties.active)
          {
              training_features->emplace_back(index_offset + sq_k);
              index_offset += properties.dimensions;
          }

          // kFeaturesP
          index_offset += InheritFeaturesIfRequired<P>(index_offset, kProperties[kFeaturesP],
                                                       p, training_features);
          // kFeaturesHalfRelativeKP
          if (p >= fe_hand_end)
              index_offset += InheritFeaturesIfRequired<HalfRelativeKP<AssociatedKing>>(index_offset, kProperties[kFeaturesHalfRelativeKP],
                                                                                        HalfRelativeKP<AssociatedKing>::MakeIndex(sq_k, p),
                                                                                        training_features);
          else
              index_offset += SkipFeatures(kProperties[kFeaturesHalfRelativeKP]);

          assert(index_offset == GetDimensions());
        }
      };

      template <Side AssociatedKing>
      constexpr FeatureProperties Factorizer<HalfKP<AssociatedKing>>::kProperties[];

    } // namespace Features
  } // namespace NNUE
} // namespace Eval

#endif  // defined(EVAL_NNUE)

#endif
