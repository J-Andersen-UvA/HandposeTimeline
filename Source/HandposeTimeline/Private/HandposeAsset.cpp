#include "HandposeAsset.h"

bool UHandposeAsset::isValid() const
{
    return !predictionHandpose.IsNull() && !postProcessedHandpose.IsNull();
}
