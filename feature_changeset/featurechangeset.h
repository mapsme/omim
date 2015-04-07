#ifndef FEATURECHANGESET_H
#define FEATURECHANGESET_H

#include "../std/string.hpp"
#include "../std/tuple.hpp"
#include "../std/map.hpp"

namespace edit
{

struct DataValue
{
    string old_value;
    string new_value;
};

enum EDataKey
{
    NAME = 1,
    ADDR_HOUSENUMBER=2,
    ADDR_POSTCODE=3
};

using TMWMLink = tuple<double /* lon */, double /* lat */, uint32_t /*type*/>;
using TChanges = map<EDataKey, DataValue>;

class FeatureDiff
{
    enum EState
    {
        CREATED, MODIFIED, DELETED
    };

    uint64_t key; // internal

    TMWMLink id;
    time_t created;
    time_t modified;
    time_t uploaded = 0;
    EState state = CREATED;

    TChanges changes;
};

class FeatureChangeset
{

public:
    FeatureChangeset();
    void CreateChange(TMWMLink const & id, TChanges const &values);
    void ModifyChange(TMWMLink const & id, TChanges const &values);
    void DeleteChange(TMWMLink const & id);
    void Find(TMWMLink const & id);

    FeatureDiff const *LookUpDiff(uint64_t key);

    void UploadChangeset();
};
}
#endif // FEATURECHANGESET_H
