/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "transport/SecureSession.h"
#include <app-common/zap-generated/cluster-objects.h>
#include <app/ClusterStateCache.h>
#include <app/InteractionModelEngine.h>
#include <app/tests/AppTestContext.h>
#include <app/util/mock/Constants.h>
#include <app/util/mock/Functions.h>
#include <controller/ReadInteraction.h>
#include <lib/support/ErrorStr.h>
#include <lib/support/UnitTestRegistration.h>
#include <lib/support/logging/CHIPLogging.h>
#include <messaging/tests/MessagingContext.h>
#include <nlunit-test.h>
#include <protocols/interaction_model/Constants.h>

using TestContext = chip::Test::AppContext;

using namespace chip;
using namespace chip::app::Clusters;
using namespace chip::Protocols;

namespace {

constexpr EndpointId kTestEndpointId       = 1;
constexpr DataVersion kDataVersion         = 5;
constexpr AttributeId kNeverEndAttributeid = Test::MockAttributeId(1);
bool expectedAttribute1                    = true;
int16_t expectedAttribute2                 = 42;
uint64_t expectedAttribute3                = 0xdeadbeef0000cafe;
uint8_t expectedAttribute4[256]            = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
};

enum ResponseDirective
{
    kSendDataResponse,
    kSendDataError
};

ResponseDirective responseDirective;

// Number of reads of TestCluster::Attributes::Int16u that we have observed.
// Every read will increment this count by 1 and return the new value.
uint16_t totalReadCount = 0;

} // namespace

namespace chip {
namespace app {
CHIP_ERROR ReadSingleClusterData(const Access::SubjectDescriptor & aSubjectDescriptor, bool aIsFabricFiltered,
                                 const ConcreteReadAttributePath & aPath, AttributeReportIBs::Builder & aAttributeReports,
                                 AttributeValueEncoder::AttributeEncodeState * apEncoderState)
{
    if (aPath.mEndpointId >= Test::kMockEndpointMin)
    {
        return Test::ReadSingleMockClusterData(aSubjectDescriptor.fabricIndex, aPath, aAttributeReports, apEncoderState);
    }

    if (responseDirective == kSendDataResponse)
    {
        if (aPath.mClusterId == app::Clusters::TestCluster::Id &&
            aPath.mAttributeId == app::Clusters::TestCluster::Attributes::ListFabricScoped::Id)
        {
            AttributeValueEncoder::AttributeEncodeState state =
                (apEncoderState == nullptr ? AttributeValueEncoder::AttributeEncodeState() : *apEncoderState);
            AttributeValueEncoder valueEncoder(aAttributeReports, aSubjectDescriptor.fabricIndex, aPath,
                                               kDataVersion /* data version */, aIsFabricFiltered, state);

            return valueEncoder.EncodeList([aSubjectDescriptor](const auto & encoder) -> CHIP_ERROR {
                app::Clusters::TestCluster::Structs::TestFabricScoped::Type val;
                val.fabricIndex = aSubjectDescriptor.fabricIndex;
                ReturnErrorOnFailure(encoder.Encode(val));
                val.fabricIndex = (val.fabricIndex == 1) ? 2 : 1;
                ReturnErrorOnFailure(encoder.Encode(val));
                return CHIP_NO_ERROR;
            });
        }
        if (aPath.mClusterId == app::Clusters::TestCluster::Id &&
            aPath.mAttributeId == app::Clusters::TestCluster::Attributes::Int16u::Id)
        {
            AttributeValueEncoder::AttributeEncodeState state =
                (apEncoderState == nullptr ? AttributeValueEncoder::AttributeEncodeState() : *apEncoderState);
            AttributeValueEncoder valueEncoder(aAttributeReports, aSubjectDescriptor.fabricIndex, aPath,
                                               kDataVersion /* data version */, aIsFabricFiltered, state);

            return valueEncoder.Encode(++totalReadCount);
        }
        if (aPath.mClusterId == app::Clusters::TestCluster::Id && aPath.mAttributeId == kNeverEndAttributeid)
        {
            AttributeValueEncoder::AttributeEncodeState state = AttributeValueEncoder::AttributeEncodeState();
            AttributeValueEncoder valueEncoder(aAttributeReports, aSubjectDescriptor.fabricIndex, aPath,
                                               kDataVersion /* data version */, aIsFabricFiltered, state);

            CHIP_ERROR err = valueEncoder.EncodeList([](const auto & encoder) -> CHIP_ERROR {
                encoder.Encode(static_cast<uint8_t>(1));
                return CHIP_ERROR_NO_MEMORY;
            });

            if (err != CHIP_NO_ERROR)
            {
                // If the err is not CHIP_NO_ERROR, means the encoding was aborted, then the valueEncoder may save its state.
                // The state is used by list chunking feature for now.
                if (apEncoderState != nullptr)
                {
                    *apEncoderState = valueEncoder.GetState();
                }
                return err;
            }
        }

        AttributeReportIB::Builder & attributeReport = aAttributeReports.CreateAttributeReport();
        ReturnErrorOnFailure(aAttributeReports.GetError());
        AttributeDataIB::Builder & attributeData = attributeReport.CreateAttributeData();
        ReturnErrorOnFailure(attributeReport.GetError());
        TestCluster::Attributes::ListStructOctetString::TypeInfo::Type value;
        TestCluster::Structs::TestListStructOctet::Type valueBuf[4];

        value = valueBuf;

        uint8_t i = 0;
        for (auto & item : valueBuf)
        {
            item.fabricIndex = i;
            i++;
        }

        attributeData.DataVersion(kDataVersion);
        ReturnErrorOnFailure(attributeData.GetError());
        AttributePathIB::Builder & attributePath = attributeData.CreatePath();
        attributePath.Endpoint(aPath.mEndpointId).Cluster(aPath.mClusterId).Attribute(aPath.mAttributeId).EndOfAttributePathIB();
        ReturnErrorOnFailure(attributePath.GetError());

        ReturnErrorOnFailure(
            DataModel::Encode(*(attributeData.GetWriter()), TLV::ContextTag(to_underlying(AttributeDataIB::Tag::kData)), value));
        ReturnErrorOnFailure(attributeData.EndOfAttributeDataIB().GetError());
        return attributeReport.EndOfAttributeReportIB().GetError();
    }

    AttributeReportIB::Builder & attributeReport = aAttributeReports.CreateAttributeReport();
    ReturnErrorOnFailure(aAttributeReports.GetError());
    AttributeStatusIB::Builder & attributeStatus = attributeReport.CreateAttributeStatus();
    AttributePathIB::Builder & attributePath     = attributeStatus.CreatePath();
    attributePath.Endpoint(aPath.mEndpointId).Cluster(aPath.mClusterId).Attribute(aPath.mAttributeId).EndOfAttributePathIB();
    ReturnErrorOnFailure(attributePath.GetError());

    StatusIB::Builder & errorStatus = attributeStatus.CreateErrorStatus();
    ReturnErrorOnFailure(attributeStatus.GetError());
    errorStatus.EncodeStatusIB(StatusIB(Protocols::InteractionModel::Status::Busy));
    attributeStatus.EndOfAttributeStatusIB();
    ReturnErrorOnFailure(attributeStatus.GetError());

    return attributeReport.EndOfAttributeReportIB().GetError();
}

bool IsClusterDataVersionEqual(const app::ConcreteClusterPath & aConcreteClusterPath, DataVersion aRequiredVersion)
{
    if (aRequiredVersion == kDataVersion)
    {
        return true;
    }
    if (Test::GetVersion() == aRequiredVersion)
    {
        return true;
    }

    return false;
}

bool IsDeviceTypeOnEndpoint(DeviceTypeId deviceType, EndpointId endpoint)
{
    return false;
}
} // namespace app
} // namespace chip

namespace {

class TestReadInteraction : public app::ReadHandler::ApplicationCallback
{
public:
    TestReadInteraction() {}

    static void TestReadAttributeResponse(nlTestSuite * apSuite, void * apContext);
    static void TestReadAttributeError(nlTestSuite * apSuite, void * apContext);
    static void TestReadAttributeTimeout(nlTestSuite * apSuite, void * apContext);
    static void TestSubscribeAttributeTimeout(nlTestSuite * apSuite, void * apContext);
    static void TestReadEventResponse(nlTestSuite * apSuite, void * apContext);
    static void TestReadFabricScopedWithoutFabricFilter(nlTestSuite * apSuite, void * apContext);
    static void TestReadFabricScopedWithFabricFilter(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_MultipleSubscriptions(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_SubscriptionAppRejection(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_MultipleReads(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_OneSubscribeMultipleReads(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_TwoSubscribesMultipleReads(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_MultipleSubscriptionsWithDataVersionFilter(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_SubscriptionAlteredReportingIntervals(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandlerResourceExhaustion_MultipleReads(nlTestSuite * apSuite, void * apContext);
    static void TestReadSubscribeAttributeResponseWithCache(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_KillOverQuotaSubscriptions(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_KillOldestSubscriptions(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_TwoParallelReads(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_TooManyPaths(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_TwoParallelReadsSecondTooManyPaths(nlTestSuite * apSuite, void * apContext);

private:
    static constexpr uint16_t kTestMinInterval = 33;
    static constexpr uint16_t kTestMaxInterval = 66;

    CHIP_ERROR OnSubscriptionRequested(app::ReadHandler & aReadHandler, Transport::SecureSession & aSecureSession)
    {
        VerifyOrReturnError(!mEmitSubscriptionError, CHIP_ERROR_INVALID_ARGUMENT);

        if (mAlterSubscriptionIntervals)
        {
            ReturnErrorOnFailure(aReadHandler.SetReportingIntervals(kTestMinInterval, kTestMaxInterval));
        }
        return CHIP_NO_ERROR;
    }

    void OnSubscriptionEstablished(app::ReadHandler & aReadHandler) { mNumActiveSubscriptions++; }

    void OnSubscriptionTerminated(app::ReadHandler & aReadHandler) { mNumActiveSubscriptions--; }

    // Issue the given number of reads in parallel and wait for them all to
    // succeed.
    static void MultipleReadHelper(nlTestSuite * apSuite, TestContext & aCtx, size_t aReadCount);

    // Establish the given number of subscriptions, then issue the given number
    // of reads in parallel and wait for them all to succeed.
    static void SubscribeThenReadHelper(nlTestSuite * apSuite, TestContext & aCtx, size_t aSubscribeCount, size_t aReadCount);

    bool mEmitSubscriptionError      = false;
    int32_t mNumActiveSubscriptions  = 0;
    bool mAlterSubscriptionIntervals = false;
};

TestReadInteraction gTestReadInteraction;

class MockInteractionModelApp : public chip::app::ClusterStateCache::Callback
{
public:
    void OnEventData(const chip::app::EventHeader & aEventHeader, chip::TLV::TLVReader * apData,
                     const chip::app::StatusIB * apStatus) override
    {}

    void OnAttributeData(const chip::app::ConcreteDataAttributePath & aPath, chip::TLV::TLVReader * apData,
                         const chip::app::StatusIB & status) override
    {
        if (status.mStatus == chip::Protocols::InteractionModel::Status::Success)
        {
            ChipLogProgress(DataManagement, "\t\t -- attribute  status sucess");
            mNumAttributeResponse++;
        }
        ChipLogProgress(DataManagement, "\t\t -- OnAttributeData is called");
    }

    void OnError(CHIP_ERROR aError) override
    {
        mError     = aError;
        mReadError = true;
    }

    void OnDone(app::ReadClient *) override {}

    void OnDeallocatePaths(chip::app::ReadPrepareParams && aReadPrepareParams) override
    {
        if (aReadPrepareParams.mpAttributePathParamsList != nullptr)
        {
            delete[] aReadPrepareParams.mpAttributePathParamsList;
        }

        if (aReadPrepareParams.mpEventPathParamsList != nullptr)
        {
            delete[] aReadPrepareParams.mpEventPathParamsList;
        }

        if (aReadPrepareParams.mpDataVersionFilterList != nullptr)
        {
            delete[] aReadPrepareParams.mpDataVersionFilterList;
        }
    }

    int mNumAttributeResponse = 0;
    bool mReadError           = false;
    CHIP_ERROR mError         = CHIP_NO_ERROR;
};

void TestReadInteraction::TestReadAttributeResponse(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx       = *static_cast<TestContext *>(apContext);
    auto sessionHandle      = ctx.GetSessionBobToAlice();
    bool onSuccessCbInvoked = false, onFailureCbInvoked = false;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [apSuite, &onSuccessCbInvoked](const app::ConcreteDataAttributePath & attributePath,
                                                      const auto & dataResponse) {
        uint8_t i = 0;
        NL_TEST_ASSERT(apSuite, attributePath.mDataVersion.HasValue() && attributePath.mDataVersion.Value() == kDataVersion);
        auto iter = dataResponse.begin();
        while (iter.Next())
        {
            auto & item = iter.GetValue();
            NL_TEST_ASSERT(apSuite, item.fabricIndex == i);
            i++;
        }
        NL_TEST_ASSERT(apSuite, i == 4);
        NL_TEST_ASSERT(apSuite, iter.GetStatus() == CHIP_NO_ERROR);
        onSuccessCbInvoked = true;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&onFailureCbInvoked](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        onFailureCbInvoked = true;
    };

    Controller::ReadAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(&ctx.GetExchangeManager(), sessionHandle,
                                                                                        kTestEndpointId, onSuccessCb, onFailureCb);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, onSuccessCbInvoked && !onFailureCbInvoked);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadSubscribeAttributeResponseWithCache(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx = *static_cast<TestContext *>(apContext);
    CHIP_ERROR err    = CHIP_NO_ERROR;
    responseDirective = kSendDataResponse;

    MockInteractionModelApp delegate;
    chip::app::ClusterStateCache cache(delegate);

    chip::app::EventPathParams eventPathParams[100];
    for (uint32_t index = 0; index < 100; index++)
    {
        eventPathParams[index].mEndpointId = Test::kMockEndpoint3;
        eventPathParams[index].mClusterId  = Test::MockClusterId(2);
        eventPathParams[index].mEventId    = 0;
    }

    chip::app::ReadPrepareParams readPrepareParams(ctx.GetSessionBobToAlice());
    readPrepareParams.mMinIntervalFloorSeconds   = 0;
    readPrepareParams.mMaxIntervalCeilingSeconds = 4;
    //
    // Test the application callback as well to ensure we get the right number of SubscriptionEstablishment/Termination
    // callbacks.
    //
    app::InteractionModelEngine::GetInstance()->RegisterReadHandlerAppCallback(&gTestReadInteraction);

    int testId = 0;

    // Read of E2C3A1(dedup), E*C3A1(E1C3A1 not exit, E2C3A1 exist), E2C3A* (5 supported attributes)
    // Expect no versions would be cached.
    {
        testId++;
        ChipLogProgress(DataManagement, "\t -- Running Read with ClusterStateCache Test ID %d", testId);
        app::ReadClient readClient(chip::app::InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(),
                                   cache.GetBufferedCallback(), chip::app::ReadClient::InteractionType::Read);
        chip::app::AttributePathParams attributePathParams1[3];
        attributePathParams1[0].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams1[0].mClusterId   = Test::MockClusterId(3);
        attributePathParams1[0].mAttributeId = Test::MockAttributeId(1);

        attributePathParams1[1].mEndpointId  = kInvalidEndpointId;
        attributePathParams1[1].mClusterId   = Test::MockClusterId(3);
        attributePathParams1[1].mAttributeId = Test::MockAttributeId(1);

        attributePathParams1[2].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams1[2].mClusterId   = Test::MockClusterId(3);
        attributePathParams1[2].mAttributeId = kInvalidAttributeId;

        readPrepareParams.mpAttributePathParamsList    = attributePathParams1;
        readPrepareParams.mAttributePathParamsListSize = 3;
        err                                            = readClient.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();
        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 6);
        NL_TEST_ASSERT(apSuite, !delegate.mReadError);
        Optional<DataVersion> version1;
        app::ConcreteClusterPath clusterPath1(Test::kMockEndpoint2, Test::MockClusterId(3));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath1, version1) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, !version1.HasValue());
        delegate.mNumAttributeResponse = 0;
    }

    // Read of E2C3A1, E2C3A2 and E3C2A2.
    // Expect no versions would be cached.
    {
        testId++;
        ChipLogProgress(DataManagement, "\t -- Running Read with ClusterStateCache Test ID %d", testId);
        app::ReadClient readClient(chip::app::InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(),
                                   cache.GetBufferedCallback(), chip::app::ReadClient::InteractionType::Read);
        chip::app::AttributePathParams attributePathParams1[3];
        attributePathParams1[0].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams1[0].mClusterId   = Test::MockClusterId(3);
        attributePathParams1[0].mAttributeId = Test::MockAttributeId(1);

        attributePathParams1[1].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams1[1].mClusterId   = Test::MockClusterId(3);
        attributePathParams1[1].mAttributeId = Test::MockAttributeId(2);

        attributePathParams1[2].mEndpointId  = Test::kMockEndpoint3;
        attributePathParams1[2].mClusterId   = Test::MockClusterId(2);
        attributePathParams1[2].mAttributeId = Test::MockAttributeId(2);

        readPrepareParams.mpAttributePathParamsList    = attributePathParams1;
        readPrepareParams.mAttributePathParamsListSize = 3;
        err                                            = readClient.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();
        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 3);
        NL_TEST_ASSERT(apSuite, !delegate.mReadError);
        Optional<DataVersion> version1;
        app::ConcreteClusterPath clusterPath1(Test::kMockEndpoint2, Test::MockClusterId(3));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath1, version1) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, !version1.HasValue());
        Optional<DataVersion> version2;
        app::ConcreteClusterPath clusterPath2(Test::kMockEndpoint3, Test::MockClusterId(2));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath2, version2) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, !version2.HasValue());

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint3, Test::MockClusterId(2), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }

        delegate.mNumAttributeResponse = 0;
    }

    // Read of E*C2A2, E2C2A2 and E3C2A2 where 2nd, 3rd concrete paths are part of first wildcard path, would be deduplicate,
    // E*C2A2 don't have wildcard attribute so no version would be cached.
    // Expect no versions would be cached.
    {
        testId++;
        ChipLogProgress(DataManagement, "\t -- Running Read with ClusterStateCache Test ID %d", testId);
        app::ReadClient readClient(chip::app::InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(),
                                   cache.GetBufferedCallback(), chip::app::ReadClient::InteractionType::Read);
        chip::app::AttributePathParams attributePathParams1[3];
        attributePathParams1[0].mEndpointId  = kInvalidEndpointId;
        attributePathParams1[0].mClusterId   = Test::MockClusterId(2);
        attributePathParams1[0].mAttributeId = Test::MockAttributeId(2);

        attributePathParams1[1].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams1[1].mClusterId   = Test::MockClusterId(2);
        attributePathParams1[1].mAttributeId = Test::MockAttributeId(2);

        attributePathParams1[2].mEndpointId  = Test::kMockEndpoint3;
        attributePathParams1[2].mClusterId   = Test::MockClusterId(2);
        attributePathParams1[2].mAttributeId = Test::MockAttributeId(2);

        readPrepareParams.mpAttributePathParamsList    = attributePathParams1;
        readPrepareParams.mAttributePathParamsListSize = 3;
        err                                            = readClient.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();
        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 2);
        NL_TEST_ASSERT(apSuite, !delegate.mReadError);
        Optional<DataVersion> version1;
        app::ConcreteClusterPath clusterPath1(Test::kMockEndpoint2, Test::MockClusterId(3));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath1, version1) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, !version1.HasValue());
        Optional<DataVersion> version2;
        app::ConcreteClusterPath clusterPath2(Test::kMockEndpoint3, Test::MockClusterId(2));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath2, version2) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, !version2.HasValue());

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint1, Test::MockClusterId(2), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) != CHIP_NO_ERROR);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(2), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint3, Test::MockClusterId(2), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }
        delegate.mNumAttributeResponse = 0;
    }

    // read of E2C2A* and E3C2A2. We cannot use the stored data versions in the cache since there is no cached version from
    // previous test. Expect cache E2C2 version
    {
        testId++;
        ChipLogProgress(DataManagement, "\t -- Running Read with ClusterStateCache Test ID %d", testId);
        app::ReadClient readClient(chip::app::InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(),
                                   cache.GetBufferedCallback(), chip::app::ReadClient::InteractionType::Read);
        chip::app::AttributePathParams attributePathParams2[2];
        attributePathParams2[0].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams2[0].mClusterId   = Test::MockClusterId(3);
        attributePathParams2[0].mAttributeId = kInvalidAttributeId;

        attributePathParams2[1].mEndpointId            = Test::kMockEndpoint3;
        attributePathParams2[1].mClusterId             = Test::MockClusterId(2);
        attributePathParams2[1].mAttributeId           = Test::MockAttributeId(2);
        readPrepareParams.mpAttributePathParamsList    = attributePathParams2;
        readPrepareParams.mAttributePathParamsListSize = 2;
        err                                            = readClient.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();
        // There are supported 2 global and 3 non-global attributes in E2C2A* and  1 E3C2A2
        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 6);
        NL_TEST_ASSERT(apSuite, !delegate.mReadError);
        Optional<DataVersion> version1;
        app::ConcreteClusterPath clusterPath1(Test::kMockEndpoint2, Test::MockClusterId(3));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath1, version1) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, version1.HasValue() && (version1.Value() == 0));
        Optional<DataVersion> version2;
        app::ConcreteClusterPath clusterPath2(Test::kMockEndpoint3, Test::MockClusterId(2));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath2, version2) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, !version2.HasValue());

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(3));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            uint64_t receivedAttribute3;
            reader.Get(receivedAttribute3);
            NL_TEST_ASSERT(apSuite, receivedAttribute3 == expectedAttribute3);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint3, Test::MockClusterId(2), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }
        delegate.mNumAttributeResponse = 0;
    }

    // Read of E2C3A1, E2C3A2, and E3C2A2. It would use the stored data versions in the cache since our subsequent read's C1A1
    // path intersects with previous cached data version Expect no E2C3 attributes in report, only E3C2A1 attribute in report
    {
        testId++;
        ChipLogProgress(DataManagement, "\t -- Running Read with ClusterStateCache Test ID %d", testId);
        app::ReadClient readClient(chip::app::InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(),
                                   cache.GetBufferedCallback(), chip::app::ReadClient::InteractionType::Read);
        chip::app::AttributePathParams attributePathParams1[3];
        attributePathParams1[0].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams1[0].mClusterId   = Test::MockClusterId(3);
        attributePathParams1[0].mAttributeId = Test::MockAttributeId(1);

        attributePathParams1[1].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams1[1].mClusterId   = Test::MockClusterId(3);
        attributePathParams1[1].mAttributeId = Test::MockAttributeId(2);

        attributePathParams1[2].mEndpointId  = Test::kMockEndpoint3;
        attributePathParams1[2].mClusterId   = Test::MockClusterId(2);
        attributePathParams1[2].mAttributeId = Test::MockAttributeId(2);

        readPrepareParams.mpAttributePathParamsList    = attributePathParams1;
        readPrepareParams.mAttributePathParamsListSize = 3;
        err                                            = readClient.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();
        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 1);
        NL_TEST_ASSERT(apSuite, !delegate.mReadError);
        Optional<DataVersion> version1;
        app::ConcreteClusterPath clusterPath1(Test::kMockEndpoint2, Test::MockClusterId(3));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath1, version1) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, version1.HasValue() && (version1.Value() == 0));
        Optional<DataVersion> version2;
        app::ConcreteClusterPath clusterPath2(Test::kMockEndpoint3, Test::MockClusterId(2));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath2, version2) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, !version2.HasValue());

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint3, Test::MockClusterId(2), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }
        delegate.mNumAttributeResponse = 0;
    }

    // Read of E2C3A* and E3C2A2. It would use the stored data versions in the cache since our subsequent read's C1A* path
    // intersects with previous cached data version Expect no C1 attributes in report, only E3C2A2 attribute in report
    {
        testId++;
        ChipLogProgress(DataManagement, "\t -- Running Read with ClusterStateCache Test ID %d", testId);
        app::ReadClient readClient(chip::app::InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(),
                                   cache.GetBufferedCallback(), chip::app::ReadClient::InteractionType::Read);
        chip::app::AttributePathParams attributePathParams2[2];
        attributePathParams2[0].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams2[0].mClusterId   = Test::MockClusterId(3);
        attributePathParams2[0].mAttributeId = kInvalidAttributeId;

        attributePathParams2[1].mEndpointId            = Test::kMockEndpoint3;
        attributePathParams2[1].mClusterId             = Test::MockClusterId(2);
        attributePathParams2[1].mAttributeId           = Test::MockAttributeId(2);
        readPrepareParams.mpAttributePathParamsList    = attributePathParams2;
        readPrepareParams.mAttributePathParamsListSize = 2;
        err                                            = readClient.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();
        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 1);
        NL_TEST_ASSERT(apSuite, !delegate.mReadError);
        Optional<DataVersion> version1;
        app::ConcreteClusterPath clusterPath1(Test::kMockEndpoint2, Test::MockClusterId(3));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath1, version1) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, version1.HasValue() && (version1.Value() == 0));
        Optional<DataVersion> version2;
        app::ConcreteClusterPath clusterPath2(Test::kMockEndpoint3, Test::MockClusterId(2));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath2, version2) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, !version2.HasValue());

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(3));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            uint64_t receivedAttribute3;
            reader.Get(receivedAttribute3);
            NL_TEST_ASSERT(apSuite, receivedAttribute3 == expectedAttribute3);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint3, Test::MockClusterId(2), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }
        delegate.mNumAttributeResponse = 0;
    }

    Test::BumpVersion();

    // Read of E2C3A1, E2C3A2 and E3C2A2. It would use the stored data versions in the cache since our subsequent read's C1A*
    // path intersects with previous cached data version, server's version is changed. Expect E2C3A1, E2C3A2 and E3C2A2 attribute in
    // report, and invalidate the cached pending and committed data version since no wildcard attributes exists in mRequestPathSet.
    {
        testId++;
        ChipLogProgress(DataManagement, "\t -- Running Read with ClusterStateCache Test ID %d", testId);
        app::ReadClient readClient(chip::app::InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(),
                                   cache.GetBufferedCallback(), chip::app::ReadClient::InteractionType::Read);
        chip::app::AttributePathParams attributePathParams1[3];
        attributePathParams1[0].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams1[0].mClusterId   = Test::MockClusterId(3);
        attributePathParams1[0].mAttributeId = Test::MockAttributeId(1);

        attributePathParams1[1].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams1[1].mClusterId   = Test::MockClusterId(3);
        attributePathParams1[1].mAttributeId = Test::MockAttributeId(2);

        attributePathParams1[2].mEndpointId  = Test::kMockEndpoint3;
        attributePathParams1[2].mClusterId   = Test::MockClusterId(2);
        attributePathParams1[2].mAttributeId = Test::MockAttributeId(2);

        readPrepareParams.mpAttributePathParamsList    = attributePathParams1;
        readPrepareParams.mAttributePathParamsListSize = 3;
        err                                            = readClient.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();
        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 3);
        NL_TEST_ASSERT(apSuite, !delegate.mReadError);
        Optional<DataVersion> version1;
        app::ConcreteClusterPath clusterPath1(Test::kMockEndpoint2, Test::MockClusterId(3));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath1, version1) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, !version1.HasValue());
        Optional<DataVersion> version2;
        app::ConcreteClusterPath clusterPath2(Test::kMockEndpoint3, Test::MockClusterId(2));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath2, version2) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, !version2.HasValue());

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint3, Test::MockClusterId(2), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }
        delegate.mNumAttributeResponse = 0;
    }

    // Read of E2C3A1, E2C3A2 and E3C2A2. It would use none stored data versions in the cache since previous read does not
    // cache any committed data version. Expect E2C3A1, E2C3A2 and E3C2A2 attribute in report
    {
        testId++;
        ChipLogProgress(DataManagement, "\t -- Running Read with ClusterStateCache Test ID %d", testId);
        app::ReadClient readClient(chip::app::InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(),
                                   cache.GetBufferedCallback(), chip::app::ReadClient::InteractionType::Read);
        chip::app::AttributePathParams attributePathParams1[3];
        attributePathParams1[0].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams1[0].mClusterId   = Test::MockClusterId(3);
        attributePathParams1[0].mAttributeId = Test::MockAttributeId(1);

        attributePathParams1[1].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams1[1].mClusterId   = Test::MockClusterId(3);
        attributePathParams1[1].mAttributeId = Test::MockAttributeId(2);

        attributePathParams1[2].mEndpointId  = Test::kMockEndpoint3;
        attributePathParams1[2].mClusterId   = Test::MockClusterId(2);
        attributePathParams1[2].mAttributeId = Test::MockAttributeId(2);

        readPrepareParams.mpAttributePathParamsList    = attributePathParams1;
        readPrepareParams.mAttributePathParamsListSize = 3;
        err                                            = readClient.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();
        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 3);
        NL_TEST_ASSERT(apSuite, !delegate.mReadError);
        Optional<DataVersion> version1;
        app::ConcreteClusterPath clusterPath1(Test::kMockEndpoint2, Test::MockClusterId(3));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath1, version1) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, !version1.HasValue());
        Optional<DataVersion> version2;
        app::ConcreteClusterPath clusterPath2(Test::kMockEndpoint3, Test::MockClusterId(2));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath2, version2) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, !version2.HasValue());

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint3, Test::MockClusterId(2), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }
        delegate.mNumAttributeResponse = 0;
    }

    // Read of E2C3A* and E3C2A2, here there is no cached data version filter
    // Expect E2C3A* attributes in report, and E3C2A2 attribute in report and cache latest data version
    {
        testId++;
        ChipLogProgress(DataManagement, "\t -- Running Read with ClusterStateCache Test ID %d", testId);
        app::ReadClient readClient(chip::app::InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(),
                                   cache.GetBufferedCallback(), chip::app::ReadClient::InteractionType::Read);
        chip::app::AttributePathParams attributePathParams2[2];
        attributePathParams2[0].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams2[0].mClusterId   = Test::MockClusterId(3);
        attributePathParams2[0].mAttributeId = kInvalidAttributeId;

        attributePathParams2[1].mEndpointId            = Test::kMockEndpoint3;
        attributePathParams2[1].mClusterId             = Test::MockClusterId(2);
        attributePathParams2[1].mAttributeId           = Test::MockAttributeId(2);
        readPrepareParams.mpAttributePathParamsList    = attributePathParams2;
        readPrepareParams.mAttributePathParamsListSize = 2;
        err                                            = readClient.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();
        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 6);
        NL_TEST_ASSERT(apSuite, !delegate.mReadError);
        Optional<DataVersion> version1;
        app::ConcreteClusterPath clusterPath1(Test::kMockEndpoint2, Test::MockClusterId(3));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath1, version1) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, version1.HasValue() && (version1.Value() == 1));
        Optional<DataVersion> version2;
        app::ConcreteClusterPath clusterPath2(Test::kMockEndpoint3, Test::MockClusterId(2));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath2, version2) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, !version2.HasValue());

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(3));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            uint64_t receivedAttribute3;
            reader.Get(receivedAttribute3);
            NL_TEST_ASSERT(apSuite, receivedAttribute3 == expectedAttribute3);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint3, Test::MockClusterId(2), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }
        delegate.mNumAttributeResponse = 0;
    }

    // Read of E2C3A* and E3C2A2, and inject a large amount of event path list, then it would try to apply previous cache
    // latest data version and construct data version list but no enough memory, finally fully rollback data version filter. Expect
    // E2C3A* attributes in report, and E3C2A2 attribute in report
    {
        testId++;
        ChipLogProgress(DataManagement, "\t -- Running Read with ClusterStateCache Test ID %d", testId);
        app::ReadClient readClient(chip::app::InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(),
                                   cache.GetBufferedCallback(), chip::app::ReadClient::InteractionType::Read);
        chip::app::AttributePathParams attributePathParams2[2];
        attributePathParams2[0].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams2[0].mClusterId   = Test::MockClusterId(3);
        attributePathParams2[0].mAttributeId = kInvalidAttributeId;

        attributePathParams2[1].mEndpointId            = Test::kMockEndpoint3;
        attributePathParams2[1].mClusterId             = Test::MockClusterId(2);
        attributePathParams2[1].mAttributeId           = Test::MockAttributeId(2);
        readPrepareParams.mpAttributePathParamsList    = attributePathParams2;
        readPrepareParams.mAttributePathParamsListSize = 2;

        readPrepareParams.mpEventPathParamsList    = eventPathParams;
        readPrepareParams.mEventPathParamsListSize = 64;

        err = readClient.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();
        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 6);
        NL_TEST_ASSERT(apSuite, !delegate.mReadError);
        Optional<DataVersion> version1;
        app::ConcreteClusterPath clusterPath1(Test::kMockEndpoint2, Test::MockClusterId(3));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath1, version1) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, version1.HasValue() && (version1.Value() == 1));
        Optional<DataVersion> version2;
        app::ConcreteClusterPath clusterPath2(Test::kMockEndpoint3, Test::MockClusterId(2));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath2, version2) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, !version2.HasValue());

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(3));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            uint64_t receivedAttribute3;
            reader.Get(receivedAttribute3);
            NL_TEST_ASSERT(apSuite, receivedAttribute3 == expectedAttribute3);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint3, Test::MockClusterId(2), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }
        delegate.mNumAttributeResponse             = 0;
        readPrepareParams.mpEventPathParamsList    = nullptr;
        readPrepareParams.mEventPathParamsListSize = 0;
    }

    Test::BumpVersion();

    // Read of E1C2A* and E2C3A* and E2C2A*, it would use C1 cached version to construct DataVersionFilter, but version has
    // changed in server. Expect E1C2A* and C2C3A* and E2C2A* attributes in report, and cache their versions
    {
        testId++;
        ChipLogProgress(DataManagement, "\t -- Running Read with ClusterStateCache Test ID %d", testId);
        app::ReadClient readClient(chip::app::InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(),
                                   cache.GetBufferedCallback(), chip::app::ReadClient::InteractionType::Read);

        chip::app::AttributePathParams attributePathParams3[3];
        attributePathParams3[0].mEndpointId  = Test::kMockEndpoint1;
        attributePathParams3[0].mClusterId   = Test::MockClusterId(2);
        attributePathParams3[0].mAttributeId = kInvalidAttributeId;

        attributePathParams3[1].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams3[1].mClusterId   = Test::MockClusterId(3);
        attributePathParams3[1].mAttributeId = kInvalidAttributeId;

        attributePathParams3[2].mEndpointId            = Test::kMockEndpoint2;
        attributePathParams3[2].mClusterId             = Test::MockClusterId(2);
        attributePathParams3[2].mAttributeId           = kInvalidAttributeId;
        readPrepareParams.mpAttributePathParamsList    = attributePathParams3;
        readPrepareParams.mAttributePathParamsListSize = 3;

        err = readClient.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();
        // E1C2A* has 3 attributes and E2C3A* has 5 attributes and E2C2A* has 4 attributes
        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 12);
        NL_TEST_ASSERT(apSuite, !delegate.mReadError);
        Optional<DataVersion> version1;
        app::ConcreteClusterPath clusterPath1(Test::kMockEndpoint2, Test::MockClusterId(3));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath1, version1) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, version1.HasValue() && (version1.Value() == 2));
        Optional<DataVersion> version2;
        app::ConcreteClusterPath clusterPath2(Test::kMockEndpoint2, Test::MockClusterId(2));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath2, version2) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, version2.HasValue() && (version2.Value() == 2));
        Optional<DataVersion> version3;
        app::ConcreteClusterPath clusterPath3(Test::kMockEndpoint1, Test::MockClusterId(2));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath3, version3) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, version3.HasValue() && (version3.Value() == 2));

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint1, Test::MockClusterId(2), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(3));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            uint64_t receivedAttribute3;
            reader.Get(receivedAttribute3);
            NL_TEST_ASSERT(apSuite, receivedAttribute3 == expectedAttribute3);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(2), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }
        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(2), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }

        delegate.mNumAttributeResponse = 0;
    }

    // Read of E1C2A*(3 attributes) and E2C3A*(5 attributes) and E2C2A*(4 attributes), and inject a large amount of event path
    // list, then it would try to apply previous cache latest data version and construct data version list with the ordering from
    // largest cluster size to smallest cluster size(C2, C3, C1) but no enough memory, finally partially rollback data version
    // filter with only C2. Expect E1C2A*, E2C2A* attributes(7 attributes) in report,
    {
        testId++;
        ChipLogProgress(DataManagement, "\t -- Running Read with ClusterStateCache Test ID %d", testId);
        app::ReadClient readClient(chip::app::InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(),
                                   cache.GetBufferedCallback(), chip::app::ReadClient::InteractionType::Read);

        chip::app::AttributePathParams attributePathParams3[3];
        attributePathParams3[0].mEndpointId  = Test::kMockEndpoint1;
        attributePathParams3[0].mClusterId   = Test::MockClusterId(2);
        attributePathParams3[0].mAttributeId = kInvalidAttributeId;

        attributePathParams3[1].mEndpointId  = Test::kMockEndpoint2;
        attributePathParams3[1].mClusterId   = Test::MockClusterId(3);
        attributePathParams3[1].mAttributeId = kInvalidAttributeId;

        attributePathParams3[2].mEndpointId            = Test::kMockEndpoint2;
        attributePathParams3[2].mClusterId             = Test::MockClusterId(2);
        attributePathParams3[2].mAttributeId           = kInvalidAttributeId;
        readPrepareParams.mpAttributePathParamsList    = attributePathParams3;
        readPrepareParams.mAttributePathParamsListSize = 3;
        readPrepareParams.mpEventPathParamsList        = eventPathParams;
        readPrepareParams.mEventPathParamsListSize     = 62;
        err                                            = readClient.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();
        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 7);
        NL_TEST_ASSERT(apSuite, !delegate.mReadError);
        Optional<DataVersion> version1;
        app::ConcreteClusterPath clusterPath1(Test::kMockEndpoint2, Test::MockClusterId(3));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath1, version1) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, version1.HasValue() && (version1.Value() == 2));
        Optional<DataVersion> version2;
        app::ConcreteClusterPath clusterPath2(Test::kMockEndpoint2, Test::MockClusterId(2));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath2, version2) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, version2.HasValue() && (version2.Value() == 2));
        Optional<DataVersion> version3;
        app::ConcreteClusterPath clusterPath3(Test::kMockEndpoint1, Test::MockClusterId(2));
        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath3, version3) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, version3.HasValue() && (version3.Value() == 2));

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint1, Test::MockClusterId(2), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(3), Test::MockAttributeId(3));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            uint64_t receivedAttribute3;
            reader.Get(receivedAttribute3);
            NL_TEST_ASSERT(apSuite, receivedAttribute3 == expectedAttribute3);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(2), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint2, Test::MockClusterId(2), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }
        delegate.mNumAttributeResponse             = 0;
        readPrepareParams.mpEventPathParamsList    = nullptr;
        readPrepareParams.mEventPathParamsListSize = 0;
    }

    // Read of E3C2 which has a oversized list attribute, MockAttributeId (4). It would use none stored data versions in the cache
    // since previous read does not cache any committed data version for E3C2, and expect to cache E3C2's version
    {
        testId++;
        ChipLogProgress(DataManagement, "\t -- Running Read with ClusterStateCache Test ID %d", testId);
        app::ReadClient readClient(chip::app::InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(),
                                   cache.GetBufferedCallback(), chip::app::ReadClient::InteractionType::Read);
        chip::app::AttributePathParams attributePathParams1[1];
        attributePathParams1[0].mEndpointId = Test::kMockEndpoint3;
        attributePathParams1[0].mClusterId  = Test::MockClusterId(2);

        readPrepareParams.mpAttributePathParamsList    = attributePathParams1;
        readPrepareParams.mAttributePathParamsListSize = 1;
        err                                            = readClient.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();
        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 6);
        NL_TEST_ASSERT(apSuite, !delegate.mReadError);
        Optional<DataVersion> version1;
        app::ConcreteClusterPath clusterPath(Test::kMockEndpoint3, Test::MockClusterId(2));

        NL_TEST_ASSERT(apSuite, cache.GetVersion(clusterPath, version1) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, version1.HasValue());

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint3, Test::MockClusterId(2), Test::MockAttributeId(1));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            bool receivedAttribute1;
            reader.Get(receivedAttribute1);
            NL_TEST_ASSERT(apSuite, receivedAttribute1 == expectedAttribute1);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint3, Test::MockClusterId(2), Test::MockAttributeId(2));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            int16_t receivedAttribute2;
            reader.Get(receivedAttribute2);
            NL_TEST_ASSERT(apSuite, receivedAttribute2 == expectedAttribute2);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint3, Test::MockClusterId(2), Test::MockAttributeId(3));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            uint64_t receivedAttribute3;
            reader.Get(receivedAttribute3);
            NL_TEST_ASSERT(apSuite, receivedAttribute3 == expectedAttribute3);
        }

        {
            app::ConcreteAttributePath attributePath(Test::kMockEndpoint3, Test::MockClusterId(2), Test::MockAttributeId(4));
            TLV::TLVReader reader;
            NL_TEST_ASSERT(apSuite, cache.Get(attributePath, reader) == CHIP_NO_ERROR);
            uint8_t receivedAttribute4[256];
            reader.GetBytes(receivedAttribute4, 256);
            NL_TEST_ASSERT(apSuite, memcmp(receivedAttribute4, expectedAttribute4, 256));
        }
        delegate.mNumAttributeResponse = 0;
    }

    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadEventResponse(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx       = *static_cast<TestContext *>(apContext);
    auto sessionHandle      = ctx.GetSessionBobToAlice();
    bool onSuccessCbInvoked = false, onFailureCbInvoked = false, onDoneCbInvoked = false;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [apSuite, &onSuccessCbInvoked](const app::EventHeader & eventHeader, const auto & EventResponse) {
        // TODO: Need to add check when IM event server integration completes
        IgnoreUnusedVariable(apSuite);
        onSuccessCbInvoked = true;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&onFailureCbInvoked](const app::EventHeader * eventHeader, CHIP_ERROR aError) {
        onFailureCbInvoked = true;
    };

    auto onDoneCb = [&onDoneCbInvoked](app::ReadClient * apReadClient) { onDoneCbInvoked = true; };

    Controller::ReadEvent<TestCluster::Events::TestEvent::DecodableType>(&ctx.GetExchangeManager(), sessionHandle, kTestEndpointId,
                                                                         onSuccessCb, onFailureCb, onDoneCb);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, !onFailureCbInvoked);
    NL_TEST_ASSERT(apSuite, onDoneCbInvoked);

    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadAttributeError(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx       = *static_cast<TestContext *>(apContext);
    auto sessionHandle      = ctx.GetSessionBobToAlice();
    bool onSuccessCbInvoked = false, onFailureCbInvoked = false;

    responseDirective = kSendDataError;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&onSuccessCbInvoked](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        onSuccessCbInvoked = true;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&onFailureCbInvoked, apSuite](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        NL_TEST_ASSERT(apSuite, aError.IsIMStatus() && app::StatusIB(aError).mStatus == Protocols::InteractionModel::Status::Busy);
        onFailureCbInvoked = true;
    };

    Controller::ReadAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(&ctx.GetExchangeManager(), sessionHandle,
                                                                                        kTestEndpointId, onSuccessCb, onFailureCb);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, !onSuccessCbInvoked && onFailureCbInvoked);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadAttributeTimeout(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx       = *static_cast<TestContext *>(apContext);
    auto sessionHandle      = ctx.GetSessionBobToAlice();
    bool onSuccessCbInvoked = false, onFailureCbInvoked = false;

    responseDirective = kSendDataError;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&onSuccessCbInvoked](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        onSuccessCbInvoked = true;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&onFailureCbInvoked, apSuite](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        NL_TEST_ASSERT(apSuite, aError == CHIP_ERROR_TIMEOUT);
        onFailureCbInvoked = true;
    };

    Controller::ReadAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(&ctx.GetExchangeManager(), sessionHandle,
                                                                                        kTestEndpointId, onSuccessCb, onFailureCb);

    ctx.ExpireSessionAliceToBob();

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 1);

    ctx.ExpireSessionBobToAlice();

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, !onSuccessCbInvoked && onFailureCbInvoked);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() == 0);

    //
    // Let's put back the sessions so that the next tests (which assume a valid initialized set of sessions)
    // can function correctly.
    //
    ctx.CreateSessionAliceToBob();
    ctx.CreateSessionBobToAlice();

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

// After client initiated subscription request, test expire session so that subscription fails to establish, and trigger the timeout
// error. Client would automatically try to resubscribe and bump the value for numResubscriptionAttemptedCalls.
void TestReadInteraction::TestSubscribeAttributeTimeout(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx       = *static_cast<TestContext *>(apContext);
    auto sessionHandle      = ctx.GetSessionBobToAlice();
    bool onSuccessCbInvoked = false, onFailureCbInvoked = false;
    responseDirective                        = kSendDataError;
    uint32_t numSubscriptionEstablishedCalls = 0, numResubscriptionAttemptedCalls = 0;
    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&onSuccessCbInvoked](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        onSuccessCbInvoked = true;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&onFailureCbInvoked, apSuite](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        NL_TEST_ASSERT(apSuite, aError == CHIP_ERROR_TIMEOUT);
        onFailureCbInvoked = true;
    };

    auto onSubscriptionEstablishedCb = [&numSubscriptionEstablishedCalls](const app::ReadClient & readClient) {
        numSubscriptionEstablishedCalls++;
    };

    auto onSubscriptionAttemptedCb = [&numResubscriptionAttemptedCalls](const app::ReadClient & readClient, CHIP_ERROR aError,
                                                                        uint32_t aNextResubscribeIntervalMsec) {
        numResubscriptionAttemptedCalls++;
    };

    Controller::SubscribeAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
        &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, 0, 20, onSubscriptionEstablishedCb,
        onSubscriptionAttemptedCb, false, true);

    ctx.ExpireSessionAliceToBob();

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 1);

    ctx.ExpireSessionBobToAlice();

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite,
                   !onSuccessCbInvoked && !onFailureCbInvoked && numSubscriptionEstablishedCalls == 0 &&
                       numResubscriptionAttemptedCalls == 1);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() == 0);

    //
    // Let's put back the sessions so that the next tests (which assume a valid initialized set of sessions)
    // can function correctly.
    //
    ctx.CreateSessionAliceToBob();
    ctx.CreateSessionBobToAlice();

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadHandler_MultipleSubscriptions(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx                        = *static_cast<TestContext *>(apContext);
    auto sessionHandle                       = ctx.GetSessionBobToAlice();
    uint32_t numSuccessCalls                 = 0;
    uint32_t numSubscriptionEstablishedCalls = 0;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&numSuccessCalls](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        numSuccessCalls++;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&apSuite](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        //
        // We shouldn't be encountering any failures in this test.
        //
        NL_TEST_ASSERT(apSuite, false);
    };

    auto onSubscriptionEstablishedCb = [&numSubscriptionEstablishedCalls](const app::ReadClient & readClient) {
        numSubscriptionEstablishedCalls++;
    };

    //
    // Test the application callback as well to ensure we get the right number of SubscriptionEstablishment/Termination
    // callbacks.
    //
    app::InteractionModelEngine::GetInstance()->RegisterReadHandlerAppCallback(&gTestReadInteraction);

    //
    // Try to issue parallel subscriptions that will exceed the value for CHIP_IM_MAX_NUM_READ_HANDLER.
    // If heap allocation is correctly setup, this should result in it successfully servicing more than the number
    // present in that define.
    //
    for (int i = 0; i < (CHIP_IM_MAX_NUM_READ_HANDLER + 1); i++)
    {
        NL_TEST_ASSERT(apSuite,
                       Controller::SubscribeAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
                           &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, 0, 20,
                           onSubscriptionEstablishedCb, nullptr, false, true) == CHIP_NO_ERROR);
    }

    // There are too many messages and the test (gcc_debug, which includes many sanity checks) will be quite slow. Note: report
    // engine is using ScheduleWork which cannot be handled by DrainAndServiceIO correctly.
    ctx.GetIOContext().DriveIOUntil(System::Clock::Seconds16(60), [&]() {
        return numSuccessCalls == (CHIP_IM_MAX_NUM_READ_HANDLER + 1) &&
            numSubscriptionEstablishedCalls == (CHIP_IM_MAX_NUM_READ_HANDLER + 1);
    });

    NL_TEST_ASSERT(apSuite, numSuccessCalls == (CHIP_IM_MAX_NUM_READ_HANDLER + 1));
    NL_TEST_ASSERT(apSuite, numSubscriptionEstablishedCalls == (CHIP_IM_MAX_NUM_READ_HANDLER + 1));
    NL_TEST_ASSERT(apSuite, gTestReadInteraction.mNumActiveSubscriptions == (CHIP_IM_MAX_NUM_READ_HANDLER + 1));

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();

    NL_TEST_ASSERT(apSuite, gTestReadInteraction.mNumActiveSubscriptions == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    app::InteractionModelEngine::GetInstance()->UnregisterReadHandlerAppCallback();
}

void TestReadInteraction::TestReadHandler_SubscriptionAppRejection(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx                        = *static_cast<TestContext *>(apContext);
    auto sessionHandle                       = ctx.GetSessionBobToAlice();
    uint32_t numSuccessCalls                 = 0;
    uint32_t numFailureCalls                 = 0;
    uint32_t numSubscriptionEstablishedCalls = 0;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&numSuccessCalls](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        numSuccessCalls++;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&numFailureCalls](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        numFailureCalls++;
    };

    auto onSubscriptionEstablishedCb = [&numSubscriptionEstablishedCalls](const app::ReadClient & readClient) {
        numSubscriptionEstablishedCalls++;
    };

    //
    // Test the application callback as well to ensure we get the right number of SubscriptionEstablishment/Termination
    // callbacks.
    //
    app::InteractionModelEngine::GetInstance()->RegisterReadHandlerAppCallback(&gTestReadInteraction);

    //
    // Test the application rejecting subscriptions.
    //
    gTestReadInteraction.mEmitSubscriptionError = true;

    NL_TEST_ASSERT(apSuite,
                   Controller::SubscribeAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
                       &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, 0, 10,
                       onSubscriptionEstablishedCb, nullptr, false, true) == CHIP_NO_ERROR);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, numSuccessCalls == 0);

    //
    // Failures won't get routed to us here since re-subscriptions are enabled by default in the Controller::SubscribeAttribute
    // implementation.
    //
    NL_TEST_ASSERT(apSuite, numFailureCalls == 0);
    NL_TEST_ASSERT(apSuite, numSubscriptionEstablishedCalls == 0);
    NL_TEST_ASSERT(apSuite, gTestReadInteraction.mNumActiveSubscriptions == 0);

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    app::InteractionModelEngine::GetInstance()->UnregisterReadHandlerAppCallback();
    gTestReadInteraction.mEmitSubscriptionError = false;
}

void TestReadInteraction::TestReadHandler_SubscriptionAlteredReportingIntervals(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx                        = *static_cast<TestContext *>(apContext);
    auto sessionHandle                       = ctx.GetSessionBobToAlice();
    uint32_t numSuccessCalls                 = 0;
    uint32_t numFailureCalls                 = 0;
    uint32_t numSubscriptionEstablishedCalls = 0;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&numSuccessCalls](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        numSuccessCalls++;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&numFailureCalls](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        numFailureCalls++;
    };

    auto onSubscriptionEstablishedCb = [&numSubscriptionEstablishedCalls, &apSuite](const app::ReadClient & readClient) {
        uint16_t minInterval = 0, maxInterval = 0;

        CHIP_ERROR err = readClient.GetReportingIntervals(minInterval, maxInterval);

        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        NL_TEST_ASSERT(apSuite, minInterval == kTestMinInterval);
        NL_TEST_ASSERT(apSuite, maxInterval == kTestMaxInterval);

        numSubscriptionEstablishedCalls++;
    };

    //
    // Test the application callback as well to ensure we get the right number of SubscriptionEstablishment/Termination
    // callbacks.
    //
    app::InteractionModelEngine::GetInstance()->RegisterReadHandlerAppCallback(&gTestReadInteraction);

    //
    // Test the server-side application altering the subscription intervals.
    //
    gTestReadInteraction.mAlterSubscriptionIntervals = true;

    NL_TEST_ASSERT(apSuite,
                   Controller::SubscribeAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
                       &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, 0, 10,
                       onSubscriptionEstablishedCb, nullptr, true) == CHIP_NO_ERROR);

    ctx.DrainAndServiceIO();

    //
    // Failures won't get routed to us here since re-subscriptions are enabled by default in the Controller::SubscribeAttribute
    // implementation.
    //
    NL_TEST_ASSERT(apSuite, numSuccessCalls != 0);
    NL_TEST_ASSERT(apSuite, numFailureCalls == 0);
    NL_TEST_ASSERT(apSuite, numSubscriptionEstablishedCalls == 1);
    NL_TEST_ASSERT(apSuite, gTestReadInteraction.mNumActiveSubscriptions == 1);

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();

    NL_TEST_ASSERT(apSuite, gTestReadInteraction.mNumActiveSubscriptions == 0);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    app::InteractionModelEngine::GetInstance()->UnregisterReadHandlerAppCallback();
    gTestReadInteraction.mAlterSubscriptionIntervals = false;
}

void TestReadInteraction::TestReadHandler_MultipleReads(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx = *static_cast<TestContext *>(apContext);

    static_assert(CHIP_IM_MAX_REPORTS_IN_FLIGHT <= CHIP_IM_MAX_NUM_READ_HANDLER,
                  "How can we have more reports in flight than read handlers?");

    MultipleReadHelper(apSuite, ctx, CHIP_IM_MAX_REPORTS_IN_FLIGHT);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();
}

void TestReadInteraction::TestReadHandler_OneSubscribeMultipleReads(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx = *static_cast<TestContext *>(apContext);

    static_assert(CHIP_IM_MAX_REPORTS_IN_FLIGHT <= CHIP_IM_MAX_NUM_READ_HANDLER,
                  "How can we have more reports in flight than read handlers?");
    static_assert(CHIP_IM_MAX_REPORTS_IN_FLIGHT > 1, "We won't do any reads");

    SubscribeThenReadHelper(apSuite, ctx, 1, CHIP_IM_MAX_REPORTS_IN_FLIGHT - 1);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();
}

void TestReadInteraction::TestReadHandler_TwoSubscribesMultipleReads(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx = *static_cast<TestContext *>(apContext);

    static_assert(CHIP_IM_MAX_REPORTS_IN_FLIGHT <= CHIP_IM_MAX_NUM_READ_HANDLER,
                  "How can we have more reports in flight than read handlers?");
    static_assert(CHIP_IM_MAX_REPORTS_IN_FLIGHT > 2, "We won't do any reads");

    SubscribeThenReadHelper(apSuite, ctx, 2, CHIP_IM_MAX_REPORTS_IN_FLIGHT - 2);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();
}

void TestReadInteraction::SubscribeThenReadHelper(nlTestSuite * apSuite, TestContext & aCtx, size_t aSubscribeCount,
                                                  size_t aReadCount)
{
    auto sessionHandle                       = aCtx.GetSessionBobToAlice();
    uint32_t numSuccessCalls                 = 0;
    uint32_t numSubscriptionEstablishedCalls = 0;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&numSuccessCalls](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        numSuccessCalls++;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&apSuite](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        //
        // We shouldn't be encountering any failures in this test.
        //
        NL_TEST_ASSERT(apSuite, false);
    };

    auto onSubscriptionEstablishedCb = [&numSubscriptionEstablishedCalls, &apSuite, &aCtx, aSubscribeCount,
                                        aReadCount](const app::ReadClient & readClient) {
        numSubscriptionEstablishedCalls++;
        if (numSubscriptionEstablishedCalls == aSubscribeCount)
        {
            MultipleReadHelper(apSuite, aCtx, aReadCount);
        }
    };

    for (size_t i = 0; i < aSubscribeCount; ++i)
    {
        NL_TEST_ASSERT(apSuite,
                       Controller::SubscribeAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
                           &aCtx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, 0, 10,
                           onSubscriptionEstablishedCb, nullptr, false, true) == CHIP_NO_ERROR);
    }

    aCtx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, numSuccessCalls == aSubscribeCount);
    NL_TEST_ASSERT(apSuite, numSubscriptionEstablishedCalls == aSubscribeCount);
}

void TestReadInteraction::MultipleReadHelper(nlTestSuite * apSuite, TestContext & aCtx, size_t aReadCount)
{
    auto sessionHandle       = aCtx.GetSessionBobToAlice();
    uint32_t numSuccessCalls = 0;
    uint32_t numFailureCalls = 0;

    responseDirective = kSendDataResponse;

    uint16_t firstExpectedResponse = totalReadCount + 1;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&apSuite, &numFailureCalls](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        numFailureCalls++;

        NL_TEST_ASSERT(apSuite, attributePath == nullptr);
    };

    for (size_t i = 0; i < aReadCount; ++i)
    {
        // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise,
        // it's not safe to do so.
        auto onSuccessCb = [&numSuccessCalls, &apSuite, firstExpectedResponse,
                            i](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
            NL_TEST_ASSERT(apSuite, dataResponse == firstExpectedResponse + i);
            numSuccessCalls++;
        };

        NL_TEST_ASSERT(apSuite,
                       Controller::ReadAttribute<TestCluster::Attributes::Int16u::TypeInfo>(
                           &aCtx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb) == CHIP_NO_ERROR);
    }

    aCtx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, numSuccessCalls == aReadCount);
    NL_TEST_ASSERT(apSuite, numFailureCalls == 0);
}

void TestReadInteraction::TestReadHandler_MultipleSubscriptionsWithDataVersionFilter(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx                        = *static_cast<TestContext *>(apContext);
    auto sessionHandle                       = ctx.GetSessionBobToAlice();
    uint32_t numSuccessCalls                 = 0;
    uint32_t numSubscriptionEstablishedCalls = 0;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [apSuite, &numSuccessCalls](const app::ConcreteDataAttributePath & attributePath,
                                                   const auto & dataResponse) {
        NL_TEST_ASSERT(apSuite, attributePath.mDataVersion.HasValue() && attributePath.mDataVersion.Value() == kDataVersion);
        numSuccessCalls++;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&apSuite](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        //
        // We shouldn't be encountering any failures in this test.
        //
        NL_TEST_ASSERT(apSuite, false);
    };

    auto onSubscriptionEstablishedCb = [&numSubscriptionEstablishedCalls](const app::ReadClient & readClient) {
        numSubscriptionEstablishedCalls++;
    };

    //
    // Try to issue parallel subscriptions that will exceed the value for CHIP_IM_MAX_NUM_READ_HANDLER.
    // If heap allocation is correctly setup, this should result in it successfully servicing more than the number
    // present in that define.
    //
    chip::Optional<chip::DataVersion> dataVersion(1);
    for (int i = 0; i < (CHIP_IM_MAX_NUM_READ_HANDLER + 1); i++)
    {
        NL_TEST_ASSERT(apSuite,
                       Controller::SubscribeAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
                           &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, 0, 10,
                           onSubscriptionEstablishedCb, nullptr, false, true, dataVersion) == CHIP_NO_ERROR);
    }

    // There are too many messages and the test (gcc_debug, which includes many sanity checks) will be quite slow. Note: report
    // engine is using ScheduleWork which cannot be handled by DrainAndServiceIO correctly.
    ctx.GetIOContext().DriveIOUntil(System::Clock::Seconds16(30), [&]() {
        return numSubscriptionEstablishedCalls == (CHIP_IM_MAX_NUM_READ_HANDLER + 1) &&
            numSuccessCalls == (CHIP_IM_MAX_NUM_READ_HANDLER + 1);
    });

    ChipLogError(Zcl, "Success call cnt: %u (expect %u) subscription cnt: %u (expect %u)", numSuccessCalls,
                 uint32_t(CHIP_IM_MAX_NUM_READ_HANDLER + 1), numSubscriptionEstablishedCalls,
                 uint32_t(CHIP_IM_MAX_NUM_READ_HANDLER + 1));

    NL_TEST_ASSERT(apSuite, numSuccessCalls == (CHIP_IM_MAX_NUM_READ_HANDLER + 1));
    NL_TEST_ASSERT(apSuite, numSubscriptionEstablishedCalls == (CHIP_IM_MAX_NUM_READ_HANDLER + 1));
    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadHandlerResourceExhaustion_MultipleReads(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx        = *static_cast<TestContext *>(apContext);
    auto sessionHandle       = ctx.GetSessionBobToAlice();
    uint32_t numSuccessCalls = 0;
    uint32_t numFailureCalls = 0;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&numSuccessCalls](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        numSuccessCalls++;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&apSuite, &numFailureCalls](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        numFailureCalls++;

        NL_TEST_ASSERT(apSuite, aError == CHIP_IM_GLOBAL_STATUS(ResourceExhausted));
        NL_TEST_ASSERT(apSuite, attributePath == nullptr);
    };

    app::InteractionModelEngine::GetInstance()->SetHandlerCapacity(0);

    NL_TEST_ASSERT(apSuite,
                   Controller::ReadAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
                       &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb) == CHIP_NO_ERROR);

    ctx.DrainAndServiceIO();

    app::InteractionModelEngine::GetInstance()->SetHandlerCapacity(-1);
    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();

    NL_TEST_ASSERT(apSuite, numSuccessCalls == 0);
    NL_TEST_ASSERT(apSuite, numFailureCalls == 1);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadFabricScopedWithoutFabricFilter(nlTestSuite * apSuite, void * apContext)
{
    /**
     *  TODO: we cannot implement the e2e read tests w/ fabric filter since the test session has only one session, and the
     * ReadSingleClusterData is not the one in real applications. We should be able to move some logic out of the ember library and
     * make it possible to have more fabrics in test setup so we can have a better test coverage.
     *
     *  NOTE: Based on the TODO above, the test is testing two separate logics:
     *   - When a fabric filtered read request is received, the server is able to pass the required fabric index to the response
     * encoder.
     *   - When a fabric filtered read request is received, the response encoder is able to encode the attribute correctly.
     */
    TestContext & ctx       = *static_cast<TestContext *>(apContext);
    auto sessionHandle      = ctx.GetSessionBobToAlice();
    bool onSuccessCbInvoked = false, onFailureCbInvoked = false;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [apSuite, &onSuccessCbInvoked](const app::ConcreteDataAttributePath & attributePath,
                                                      const auto & dataResponse) {
        size_t len = 0;

        NL_TEST_ASSERT(apSuite, dataResponse.ComputeSize(&len) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, len > 1);

        onSuccessCbInvoked = true;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&onFailureCbInvoked](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        onFailureCbInvoked = true;
    };

    Controller::ReadAttribute<TestCluster::Attributes::ListFabricScoped::TypeInfo>(
        &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, false /* fabric filtered */);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, onSuccessCbInvoked && !onFailureCbInvoked);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadFabricScopedWithFabricFilter(nlTestSuite * apSuite, void * apContext)
{
    /**
     *  TODO: we cannot implement the e2e read tests w/ fabric filter since the test session has only one session, and the
     * ReadSingleClusterData is not the one in real applications. We should be able to move some logic out of the ember library and
     * make it possible to have more fabrics in test setup so we can have a better test coverage.
     *
     *  NOTE: Based on the TODO above, the test is testing two separate logics:
     *   - When a fabric filtered read request is received, the server is able to pass the required fabric index to the response
     * encoder.
     *   - When a fabric filtered read request is received, the response encoder is able to encode the attribute correctly.
     */
    TestContext & ctx       = *static_cast<TestContext *>(apContext);
    auto sessionHandle      = ctx.GetSessionBobToAlice();
    bool onSuccessCbInvoked = false, onFailureCbInvoked = false;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [apSuite, &onSuccessCbInvoked](const app::ConcreteDataAttributePath & attributePath,
                                                      const auto & dataResponse) {
        size_t len = 0;

        NL_TEST_ASSERT(apSuite, dataResponse.ComputeSize(&len) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, len == 1);

        // TODO: Uncomment the following code after we have fabric support in unit tests.
        /*
        auto iter = dataResponse.begin();
        if (iter.Next())
        {
            auto & item = iter.GetValue();
            NL_TEST_ASSERT(apSuite, item.fabricIndex == 1);
        }
        */
        onSuccessCbInvoked = true;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&onFailureCbInvoked](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        onFailureCbInvoked = true;
    };

    Controller::ReadAttribute<TestCluster::Attributes::ListFabricScoped::TypeInfo>(
        &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, true /* fabric filtered */);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, onSuccessCbInvoked && !onFailureCbInvoked);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

namespace SubscriptionPathQuotaHelpers {
class TestReadCallback : public app::ReadClient::Callback
{
public:
    TestReadCallback() {}
    void OnAttributeData(const app::ConcreteDataAttributePath & aPath, TLV::TLVReader * apData,
                         const app::StatusIB & aStatus) override
    {
        if (apData != nullptr)
        {
            mAttributeCount++;
        }
    }

    void OnDone(app::ReadClient *) override { mOnDone++; }

    void OnReportEnd() override { mOnReportEnd++; }

    void OnError(CHIP_ERROR aError) override
    {
        mOnError++;
        mLastError = aError;
    }

    void OnSubscriptionEstablished(SubscriptionId aSubscriptionId) override { mOnSubscriptionEstablishedCount++; }

    void ClearCounters()
    {
        mAttributeCount                 = 0;
        mOnReportEnd                    = 0;
        mOnSubscriptionEstablishedCount = 0;
        mOnDone                         = 0;
        mOnError                        = 0;
        mLastError                      = CHIP_NO_ERROR;
    }

    int32_t mAttributeCount                 = 0;
    int32_t mOnReportEnd                    = 0;
    int32_t mOnSubscriptionEstablishedCount = 0;
    int32_t mOnDone                         = 0;
    int32_t mOnError                        = 0;
    CHIP_ERROR mLastError                   = CHIP_NO_ERROR;
};

class TestPerpetualListReadCallback : public app::ReadClient::Callback
{
public:
    TestPerpetualListReadCallback() {}
    void OnAttributeData(const app::ConcreteDataAttributePath & aPath, TLV::TLVReader * apData,
                         const app::StatusIB & aStatus) override
    {
        if (apData != nullptr)
        {
            reportsReceived++;
            app::AttributePathParams path;
            path.mEndpointId  = aPath.mEndpointId;
            path.mClusterId   = aPath.mClusterId;
            path.mAttributeId = aPath.mAttributeId;
            app::InteractionModelEngine::GetInstance()->GetReportingEngine().SetDirty(path);
        }
    }

    void OnDone(chip::app::ReadClient *) override {}

    int32_t reportsReceived = 0;
};

void EstablishReadOrSubscriptions(nlTestSuite * apSuite, SessionHandle sessionHandle, int32_t numSubs, int32_t pathPerSub,
                                  app::AttributePathParams path, app::ReadClient::InteractionType type,
                                  app::ReadClient::Callback * callback, std::vector<std::unique_ptr<app::ReadClient>> & readClients)
{
    std::vector<app::AttributePathParams> attributePaths(pathPerSub, path);

    app::ReadPrepareParams readParams(sessionHandle);
    readParams.mpAttributePathParamsList    = attributePaths.data();
    readParams.mAttributePathParamsListSize = pathPerSub;
    if (type == app::ReadClient::InteractionType::Subscribe)
    {
        readParams.mMaxIntervalCeilingSeconds = 1;
        readParams.mKeepSubscriptions         = true;
    }

    for (int32_t i = 0; i < numSubs; i++)
    {
        std::unique_ptr<app::ReadClient> readClient =
            std::make_unique<app::ReadClient>(app::InteractionModelEngine::GetInstance(),
                                              app::InteractionModelEngine::GetInstance()->GetExchangeManager(), *callback, type);
        NL_TEST_ASSERT(apSuite, readClient->SendRequest(readParams) == CHIP_NO_ERROR);
        readClients.push_back(std::move(readClient));
    }
}

} // namespace SubscriptionPathQuotaHelpers

void TestReadInteraction::TestReadHandler_KillOverQuotaSubscriptions(nlTestSuite * apSuite, void * apContext)
{
    // Note: We cannot use ctx.DrainAndServiceIO() since the perpetual read will make DrainAndServiceIO never return.
    using namespace SubscriptionPathQuotaHelpers;
    TestContext & ctx  = *static_cast<TestContext *>(apContext);
    auto sessionHandle = ctx.GetSessionBobToAlice();

    const int32_t kExpectedParallelSubs =
        app::InteractionModelEngine::kMinSupportedSubscriptionsPerFabric * ctx.GetFabricTable().FabricCount();
    const int32_t kExpectedParallelPaths = kExpectedParallelSubs * app::InteractionModelEngine::kMinSupportedPathsPerSubscription;

    app::InteractionModelEngine::GetInstance()->RegisterReadHandlerAppCallback(&gTestReadInteraction);

    // Here, we set up two background perpetual read requests to simulate parallel Read + Subscriptions.
    // We don't care about the data read, we only care about the existence of such read transactions.
    TestReadCallback readCallback;
    TestReadCallback readCallbackFabric2;
    TestPerpetualListReadCallback perpetualReadCallback;
    std::vector<std::unique_ptr<app::ReadClient>> readClients;

    EstablishReadOrSubscriptions(apSuite, ctx.GetSessionAliceToBob(), 1, 1,
                                 app::AttributePathParams(kTestEndpointId, TestCluster::Id, kNeverEndAttributeid),
                                 app::ReadClient::InteractionType::Read, &perpetualReadCallback, readClients);
    EstablishReadOrSubscriptions(apSuite, ctx.GetSessionBobToAlice(), 1, 1,
                                 app::AttributePathParams(kTestEndpointId, TestCluster::Id, kNeverEndAttributeid),
                                 app::ReadClient::InteractionType::Read, &perpetualReadCallback, readClients);
    ctx.GetIOContext().DriveIOUntil(System::Clock::Seconds16(5), [&]() {
        return app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers(app::ReadHandler::InteractionType::Read) == 2;
    });
    // Ensure our read transactions are established.
    NL_TEST_ASSERT(apSuite,
                   app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers(app::ReadHandler::InteractionType::Read) ==
                       2);

    // Intentially establish subscriptions using exceeded resources.
    app::InteractionModelEngine::GetInstance()->SetForceHandlerQuota(false);
    //
    // We establish 1 subscription that exceeds the minimum supported paths (but is still established since the
    // target has sufficient resources), and kExpectedParallelSubs subscriptions that conform to the minimum
    // supported paths. This sets the stage to make it possible to test eviction of subscriptions that are in violation
    // of the minimum later below.
    //
    // Subscription A
    EstablishReadOrSubscriptions(apSuite, ctx.GetSessionBobToAlice(), 1,
                                 app::InteractionModelEngine::kMinSupportedPathsPerSubscription + 1,
                                 app::AttributePathParams(kTestEndpointId, TestCluster::Id, TestCluster::Attributes::Int16u::Id),
                                 app::ReadClient::InteractionType::Subscribe, &readCallback, readClients);
    // Subscription B
    EstablishReadOrSubscriptions(apSuite, ctx.GetSessionBobToAlice(), kExpectedParallelSubs,
                                 app::InteractionModelEngine::kMinSupportedPathsPerSubscription,
                                 app::AttributePathParams(kTestEndpointId, TestCluster::Id, TestCluster::Attributes::Int16u::Id),
                                 app::ReadClient::InteractionType::Subscribe, &readCallback, readClients);

    // There are too many messages and the test (gcc_debug, which includes many sanity checks) will be quite slow. Note: report
    // engine is using ScheduleWork which cannot be handled by DrainAndServiceIO correctly.
    ctx.GetIOContext().DriveIOUntil(System::Clock::Seconds16(5), [&]() {
        return readCallback.mOnSubscriptionEstablishedCount == kExpectedParallelSubs + 1 &&
            readCallback.mAttributeCount ==
            static_cast<int32_t>(kExpectedParallelSubs * app::InteractionModelEngine::kMinSupportedPathsPerSubscription +
                                 app::InteractionModelEngine::kMinSupportedPathsPerSubscription + 1);
    });

    NL_TEST_ASSERT(apSuite,
                   readCallback.mAttributeCount ==
                       static_cast<int32_t>(kExpectedParallelSubs * app::InteractionModelEngine::kMinSupportedPathsPerSubscription +
                                            app::InteractionModelEngine::kMinSupportedPathsPerSubscription + 1));
    NL_TEST_ASSERT(apSuite, readCallback.mOnSubscriptionEstablishedCount == kExpectedParallelSubs + 1);

    // We have set up the environment for testing the evicting logic.
    // We now have a full stable of subscriptions setup AND we've artificially limited the capacity, creation of further
    // subscriptions will require the eviction of existing subscriptions, OR potential rejection of the subscription if it exceeds
    // minimas.
    app::InteractionModelEngine::GetInstance()->SetForceHandlerQuota(true);
    app::InteractionModelEngine::GetInstance()->SetHandlerCapacityForSubscriptions(kExpectedParallelSubs);
    app::InteractionModelEngine::GetInstance()->SetPathPoolCapacityForSubscriptions(kExpectedParallelPaths);

    // Part 1: Test per subscription minimas.
    // Rejection of the subscription that exceeds minimas.
    {
        TestReadCallback callback;
        std::vector<std::unique_ptr<app::ReadClient>> outReadClient;
        EstablishReadOrSubscriptions(
            apSuite, ctx.GetSessionBobToAlice(), 1, app::InteractionModelEngine::kMinSupportedPathsPerSubscription + 1,
            app::AttributePathParams(kTestEndpointId, TestCluster::Id, TestCluster::Attributes::Int16u::Id),
            app::ReadClient::InteractionType::Subscribe, &callback, outReadClient);

        ctx.GetIOContext().DriveIOUntil(System::Clock::Seconds16(5), [&]() { return callback.mOnError == 1; });

        // Over-sized request after used all paths will receive Paths Exhausted status code.
        NL_TEST_ASSERT(apSuite, callback.mOnError == 1);
        NL_TEST_ASSERT(apSuite, callback.mLastError == CHIP_IM_GLOBAL_STATUS(PathsExhausted));
    }

    // This next test validates that a compliant subscription request will kick out an existing subscription (arguably, the one that
    // was previously established with more paths than the limit per fabric)
    {
        EstablishReadOrSubscriptions(
            apSuite, ctx.GetSessionBobToAlice(), 1, app::InteractionModelEngine::kMinSupportedPathsPerSubscription,
            app::AttributePathParams(kTestEndpointId, TestCluster::Id, TestCluster::Attributes::Int16u::Id),
            app::ReadClient::InteractionType::Subscribe, &readCallback, readClients);

        readCallback.ClearCounters();
        // Run until the new subscription got setup fully as viewed by the client.
        ctx.GetIOContext().DriveIOUntil(System::Clock::Seconds16(5), [&]() {
            return readCallback.mOnSubscriptionEstablishedCount == 1 &&
                readCallback.mAttributeCount == app::InteractionModelEngine::kMinSupportedPathsPerSubscription;
        });

        // This read handler should evict some existing subscriptions for enough space.
        // Validate that the new subscription got setup fully as viewed by the client. And we will validate we handled this
        // subscription by evicting the correct subscriptions later.
        NL_TEST_ASSERT(apSuite, readCallback.mOnSubscriptionEstablishedCount == 1);
        NL_TEST_ASSERT(apSuite, readCallback.mAttributeCount == app::InteractionModelEngine::kMinSupportedPathsPerSubscription);
    }

    // Validate we evicted the right subscription for handling the new subscription above.
    // We should used **exactly** all resources for subscriptions if we have evicted the correct subscription, and we validate the
    // number of used paths by mark all subscriptions as dirty, and count the number of received reports.
    {
        app::AttributePathParams path;
        path.mEndpointId  = kTestEndpointId;
        path.mClusterId   = TestCluster::Id;
        path.mAttributeId = TestCluster::Attributes::Int16u::Id;
        app::InteractionModelEngine::GetInstance()->GetReportingEngine().SetDirty(path);
    }
    readCallback.ClearCounters();

    // Run until all subscriptions are clean.
    ctx.GetIOContext().DriveIOUntil(System::Clock::Seconds16(60),
                                    [&]() { return app::InteractionModelEngine::GetInstance()->GetNumDirtySubscriptions() == 0; });

    // Before the above subscription, we have one subscription with kMinSupportedPathsPerSubscription + 1 paths, we should evict
    // that subscription before evicting any other subscriptions, which will result we used exactly kExpectedParallelPaths and have
    // exactly kExpectedParallelSubs.
    // We have exactly one subscription than uses more resources than others, so the interaction model must evict it first, and we
    // will have exactly kExpectedParallelPaths only when that subscription have been evicted. We use this indirect method to verify
    // the subscriptions since the read client won't shutdown until the timeout fired.
    NL_TEST_ASSERT(apSuite, readCallback.mAttributeCount == kExpectedParallelPaths);
    NL_TEST_ASSERT(apSuite,
                   app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers(
                       app::ReadHandler::InteractionType::Subscribe) == static_cast<uint32_t>(kExpectedParallelSubs));

    // Part 2: Testing per fabric minimas.
    // Validate we have more than kMinSupportedSubscriptionsPerFabric subscriptions for testing per fabric minimas.
    NL_TEST_ASSERT(apSuite,
                   app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers(
                       app::ReadHandler::InteractionType::Subscribe, ctx.GetAliceFabricIndex()) >
                       app::InteractionModelEngine::kMinSupportedSubscriptionsPerFabric);

    // The following check will trigger the logic in im to kill the read handlers that use more paths than the limit per fabric.
    {
        EstablishReadOrSubscriptions(
            apSuite, ctx.GetSessionAliceToBob(), app::InteractionModelEngine::kMinSupportedSubscriptionsPerFabric,
            app::InteractionModelEngine::kMinSupportedPathsPerSubscription,
            app::AttributePathParams(kTestEndpointId, TestCluster::Id, TestCluster::Attributes::Int16u::Id),
            app::ReadClient::InteractionType::Subscribe, &readCallbackFabric2, readClients);

        // Run until we have established the subscriptions.
        ctx.GetIOContext().DriveIOUntil(System::Clock::Seconds16(5), [&]() {
            return readCallbackFabric2.mOnSubscriptionEstablishedCount ==
                app::InteractionModelEngine::kMinSupportedSubscriptionsPerFabric &&
                readCallbackFabric2.mAttributeCount ==
                app::InteractionModelEngine::kMinSupportedPathsPerSubscription *
                    app::InteractionModelEngine::kMinSupportedSubscriptionsPerFabric;
        });

        // Verify the subscriptions are established successfully. We will check if we evicted the expected subscriptions later.
        NL_TEST_ASSERT(apSuite,
                       readCallbackFabric2.mOnSubscriptionEstablishedCount ==
                           app::InteractionModelEngine::kMinSupportedSubscriptionsPerFabric);
        NL_TEST_ASSERT(apSuite,
                       readCallbackFabric2.mAttributeCount ==
                           app::InteractionModelEngine::kMinSupportedPathsPerSubscription *
                               app::InteractionModelEngine::kMinSupportedSubscriptionsPerFabric);
    }

    // Validate the subscriptions are handled by evicting one or more subscriptions from Fabric A.
    {
        app::AttributePathParams path;
        path.mEndpointId  = kTestEndpointId;
        path.mClusterId   = TestCluster::Id;
        path.mAttributeId = TestCluster::Attributes::Int16u::Id;
        app::InteractionModelEngine::GetInstance()->GetReportingEngine().SetDirty(path);
    }
    readCallback.ClearCounters();
    readCallbackFabric2.ClearCounters();

    // Run until all subscriptions are clean.
    ctx.GetIOContext().DriveIOUntil(System::Clock::Seconds16(60),
                                    [&]() { return app::InteractionModelEngine::GetInstance()->GetNumDirtySubscriptions() == 0; });

    // Some subscriptions on fabric 1 should be evicted since fabric 1 is using more resources than the limits.
    NL_TEST_ASSERT(apSuite,
                   readCallback.mAttributeCount ==
                       app::InteractionModelEngine::kMinSupportedPathsPerSubscription *
                           app::InteractionModelEngine::kMinSupportedSubscriptionsPerFabric);
    NL_TEST_ASSERT(apSuite,
                   readCallbackFabric2.mAttributeCount ==
                       app::InteractionModelEngine::kMinSupportedPathsPerSubscription *
                           app::InteractionModelEngine::kMinSupportedSubscriptionsPerFabric);
    NL_TEST_ASSERT(apSuite,
                   app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers(
                       app::ReadHandler::InteractionType::Subscribe, ctx.GetAliceFabricIndex()) ==
                       app::InteractionModelEngine::kMinSupportedSubscriptionsPerFabric);
    NL_TEST_ASSERT(apSuite,
                   app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers(
                       app::ReadHandler::InteractionType::Subscribe, ctx.GetBobFabricIndex()) ==
                       app::InteractionModelEngine::kMinSupportedSubscriptionsPerFabric);

    // Ensure our read transactions are still alive.
    NL_TEST_ASSERT(apSuite,
                   app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers(app::ReadHandler::InteractionType::Read) ==
                       2);

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();
    ctx.DrainAndServiceIO();

    // Shutdown all clients
    readClients.clear();

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
    app::InteractionModelEngine::GetInstance()->SetForceHandlerQuota(false);
    app::InteractionModelEngine::GetInstance()->SetHandlerCapacityForSubscriptions(-1);
    app::InteractionModelEngine::GetInstance()->SetPathPoolCapacityForSubscriptions(-1);
}

void TestReadInteraction::TestReadHandler_KillOldestSubscriptions(nlTestSuite * apSuite, void * apContext)
{
    using namespace SubscriptionPathQuotaHelpers;
    TestContext & ctx  = *static_cast<TestContext *>(apContext);
    auto sessionHandle = ctx.GetSessionBobToAlice();

    const int32_t kExpectedParallelSubs =
        app::InteractionModelEngine::kMinSupportedSubscriptionsPerFabric * ctx.GetFabricTable().FabricCount();
    const int32_t kExpectedParallelPaths = kExpectedParallelSubs * app::InteractionModelEngine::kMinSupportedPathsPerSubscription;

    app::InteractionModelEngine::GetInstance()->RegisterReadHandlerAppCallback(&gTestReadInteraction);

    TestReadCallback readCallback;
    std::vector<std::unique_ptr<app::ReadClient>> readClients;

    app::InteractionModelEngine::GetInstance()->SetForceHandlerQuota(true);
    app::InteractionModelEngine::GetInstance()->SetHandlerCapacityForSubscriptions(kExpectedParallelSubs);
    app::InteractionModelEngine::GetInstance()->SetPathPoolCapacityForSubscriptions(kExpectedParallelPaths);

    // This should just use all availbale resources.
    EstablishReadOrSubscriptions(apSuite, ctx.GetSessionBobToAlice(), kExpectedParallelSubs,
                                 app::InteractionModelEngine::kMinSupportedPathsPerSubscription,
                                 app::AttributePathParams(kTestEndpointId, TestCluster::Id, TestCluster::Attributes::Int16u::Id),
                                 app::ReadClient::InteractionType::Subscribe, &readCallback, readClients);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite,
                   readCallback.mAttributeCount ==
                       kExpectedParallelSubs *
                           static_cast<int32_t>(app::InteractionModelEngine::kMinSupportedPathsPerSubscription));
    NL_TEST_ASSERT(apSuite, readCallback.mOnSubscriptionEstablishedCount == kExpectedParallelSubs);
    NL_TEST_ASSERT(apSuite,
                   app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() ==
                       static_cast<size_t>(kExpectedParallelSubs));

    // The following check will trigger the logic in im to kill the read handlers that uses more paths than the limit per fabric.
    {
        TestReadCallback callback;
        std::vector<std::unique_ptr<app::ReadClient>> outReadClient;
        EstablishReadOrSubscriptions(
            apSuite, ctx.GetSessionBobToAlice(), 1, app::InteractionModelEngine::kMinSupportedPathsPerSubscription + 1,
            app::AttributePathParams(kTestEndpointId, TestCluster::Id, TestCluster::Attributes::Int16u::Id),
            app::ReadClient::InteractionType::Subscribe, &callback, outReadClient);

        ctx.DrainAndServiceIO();

        // Over-sized request after used all paths will receive Paths Exhausted status code.
        NL_TEST_ASSERT(apSuite, callback.mOnError == 1);
        NL_TEST_ASSERT(apSuite, callback.mLastError == CHIP_IM_GLOBAL_STATUS(PathsExhausted));
    }

    // The following check will trigger the logic in im to kill the read handlers that uses more paths than the limit per fabric.
    {
        EstablishReadOrSubscriptions(
            apSuite, ctx.GetSessionBobToAlice(), 1, app::InteractionModelEngine::kMinSupportedPathsPerSubscription,
            app::AttributePathParams(kTestEndpointId, TestCluster::Id, TestCluster::Attributes::Int16u::Id),
            app::ReadClient::InteractionType::Subscribe, &readCallback, readClients);
        readCallback.ClearCounters();

        ctx.DrainAndServiceIO();

        // This read handler should evict some existing subscriptions for enough space
        NL_TEST_ASSERT(apSuite, readCallback.mOnSubscriptionEstablishedCount == 1);
        NL_TEST_ASSERT(apSuite, readCallback.mAttributeCount == app::InteractionModelEngine::kMinSupportedPathsPerSubscription);
        NL_TEST_ASSERT(apSuite,
                       app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() ==
                           static_cast<size_t>(kExpectedParallelSubs));
    }

    {
        app::AttributePathParams path;
        path.mEndpointId  = kTestEndpointId;
        path.mClusterId   = TestCluster::Id;
        path.mAttributeId = TestCluster::Attributes::Int16u::Id;
        app::InteractionModelEngine::GetInstance()->GetReportingEngine().SetDirty(path);
    }
    readCallback.ClearCounters();
    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, readCallback.mAttributeCount <= kExpectedParallelPaths);

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();
    ctx.DrainAndServiceIO();

    // Shutdown all clients
    readClients.clear();

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
    app::InteractionModelEngine::GetInstance()->SetForceHandlerQuota(false);
    app::InteractionModelEngine::GetInstance()->SetHandlerCapacityForSubscriptions(-1);
    app::InteractionModelEngine::GetInstance()->SetPathPoolCapacityForSubscriptions(-1);
}

void TestReadInteraction::TestReadHandler_TwoParallelReads(nlTestSuite * apSuite, void * apContext)
{
    using namespace chip::app;

    TestContext & ctx = *static_cast<TestContext *>(apContext);

    chip::Messaging::ReliableMessageMgr * rm = ctx.GetExchangeManager().GetReliableMessageMgr();
    // Shouldn't have anything in the retransmit table when starting the test.
    NL_TEST_ASSERT(apSuite, rm->TestGetCountRetransTable() == 0);

    auto * engine = app::InteractionModelEngine::GetInstance();
    engine->SetForceHandlerQuota(true);

    app::ReadPrepareParams readPrepareParams(ctx.GetSessionBobToAlice());
    // Read full wildcard paths, repeat twice to ensure chunking.
    chip::app::AttributePathParams attributePathParams[2];
    readPrepareParams.mpAttributePathParamsList    = attributePathParams;
    readPrepareParams.mAttributePathParamsListSize = ArraySize(attributePathParams);

    {
        MockInteractionModelApp delegate1;
        NL_TEST_ASSERT(apSuite, delegate1.mNumAttributeResponse == 0);
        NL_TEST_ASSERT(apSuite, !delegate1.mReadError);
        app::ReadClient readClient1(InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(), delegate1,
                                    ReadClient::InteractionType::Read);

        MockInteractionModelApp delegate2;
        NL_TEST_ASSERT(apSuite, delegate2.mNumAttributeResponse == 0);
        NL_TEST_ASSERT(apSuite, !delegate2.mReadError);
        ReadClient readClient2(InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(), delegate2,
                               ReadClient::InteractionType::Read);

        CHIP_ERROR err = readClient1.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        err = readClient2.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();

        NL_TEST_ASSERT(apSuite, delegate1.mNumAttributeResponse != 0);
        NL_TEST_ASSERT(apSuite, !delegate1.mReadError);

        NL_TEST_ASSERT(apSuite, delegate2.mNumAttributeResponse == 0);
        NL_TEST_ASSERT(apSuite, delegate2.mReadError);

        StatusIB status(delegate2.mError);
        NL_TEST_ASSERT(apSuite, status.mStatus == Protocols::InteractionModel::Status::Busy);
    }

    NL_TEST_ASSERT(apSuite, engine->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
    engine->SetForceHandlerQuota(false);
}

// Needs to be larger than our plausible path pool.
constexpr size_t sTooLargePathCount = 200;

void TestReadInteraction::TestReadHandler_TooManyPaths(nlTestSuite * apSuite, void * apContext)
{
    using namespace chip::app;

    TestContext & ctx = *static_cast<TestContext *>(apContext);

    chip::Messaging::ReliableMessageMgr * rm = ctx.GetExchangeManager().GetReliableMessageMgr();
    // Shouldn't have anything in the retransmit table when starting the test.
    NL_TEST_ASSERT(apSuite, rm->TestGetCountRetransTable() == 0);

    auto * engine = InteractionModelEngine::GetInstance();
    engine->SetForceHandlerQuota(true);

    ReadPrepareParams readPrepareParams(ctx.GetSessionBobToAlice());
    // Needs to be larger than our plausible path pool.
    chip::app::AttributePathParams attributePathParams[sTooLargePathCount];
    readPrepareParams.mpAttributePathParamsList    = attributePathParams;
    readPrepareParams.mAttributePathParamsListSize = ArraySize(attributePathParams);

    {
        MockInteractionModelApp delegate;
        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 0);
        NL_TEST_ASSERT(apSuite, !delegate.mReadError);
        ReadClient readClient(InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(), delegate,
                              ReadClient::InteractionType::Read);

        CHIP_ERROR err = readClient.SendRequest(readPrepareParams);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();

        NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 0);
        NL_TEST_ASSERT(apSuite, delegate.mReadError);

        StatusIB status(delegate.mError);
        NL_TEST_ASSERT(apSuite, status.mStatus == Protocols::InteractionModel::Status::PathsExhausted);
    }

    NL_TEST_ASSERT(apSuite, engine->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
    engine->SetForceHandlerQuota(false);
}

void TestReadInteraction::TestReadHandler_TwoParallelReadsSecondTooManyPaths(nlTestSuite * apSuite, void * apContext)
{
    using namespace chip::app;

    TestContext & ctx = *static_cast<TestContext *>(apContext);

    chip::Messaging::ReliableMessageMgr * rm = ctx.GetExchangeManager().GetReliableMessageMgr();
    // Shouldn't have anything in the retransmit table when starting the test.
    NL_TEST_ASSERT(apSuite, rm->TestGetCountRetransTable() == 0);

    auto * engine = InteractionModelEngine::GetInstance();
    engine->SetForceHandlerQuota(true);

    {
        MockInteractionModelApp delegate1;
        NL_TEST_ASSERT(apSuite, delegate1.mNumAttributeResponse == 0);
        NL_TEST_ASSERT(apSuite, !delegate1.mReadError);
        ReadClient readClient1(InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(), delegate1,
                               ReadClient::InteractionType::Read);

        MockInteractionModelApp delegate2;
        NL_TEST_ASSERT(apSuite, delegate2.mNumAttributeResponse == 0);
        NL_TEST_ASSERT(apSuite, !delegate2.mReadError);
        ReadClient readClient2(InteractionModelEngine::GetInstance(), &ctx.GetExchangeManager(), delegate2,
                               ReadClient::InteractionType::Read);

        ReadPrepareParams readPrepareParams1(ctx.GetSessionBobToAlice());
        // Read full wildcard paths, repeat twice to ensure chunking.
        chip::app::AttributePathParams attributePathParams1[2];
        readPrepareParams1.mpAttributePathParamsList    = attributePathParams1;
        readPrepareParams1.mAttributePathParamsListSize = ArraySize(attributePathParams1);

        CHIP_ERROR err = readClient1.SendRequest(readPrepareParams1);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ReadPrepareParams readPrepareParams2(ctx.GetSessionBobToAlice());
        // Read full wildcard paths, repeat twice to ensure chunking.
        chip::app::AttributePathParams attributePathParams2[sTooLargePathCount];
        readPrepareParams2.mpAttributePathParamsList    = attributePathParams2;
        readPrepareParams2.mAttributePathParamsListSize = ArraySize(attributePathParams2);

        err = readClient2.SendRequest(readPrepareParams2);
        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        ctx.DrainAndServiceIO();

        NL_TEST_ASSERT(apSuite, delegate1.mNumAttributeResponse != 0);
        NL_TEST_ASSERT(apSuite, !delegate1.mReadError);

        NL_TEST_ASSERT(apSuite, delegate2.mNumAttributeResponse == 0);
        NL_TEST_ASSERT(apSuite, delegate2.mReadError);

        StatusIB status(delegate2.mError);
        NL_TEST_ASSERT(apSuite, status.mStatus == Protocols::InteractionModel::Status::PathsExhausted);
    }

    NL_TEST_ASSERT(apSuite, engine->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
    engine->SetForceHandlerQuota(false);
}

// clang-format off
const nlTest sTests[] =
{
    NL_TEST_DEF("TestReadAttributeResponse", TestReadInteraction::TestReadAttributeResponse),
    NL_TEST_DEF("TestReadEventResponse", TestReadInteraction::TestReadEventResponse),
    NL_TEST_DEF("TestReadAttributeError", TestReadInteraction::TestReadAttributeError),
    NL_TEST_DEF("TestReadFabricScopedWithoutFabricFilter", TestReadInteraction::TestReadFabricScopedWithoutFabricFilter),
    NL_TEST_DEF("TestReadFabricScopedWithFabricFilter", TestReadInteraction::TestReadFabricScopedWithFabricFilter),
    NL_TEST_DEF("TestReadHandler_MultipleSubscriptions", TestReadInteraction::TestReadHandler_MultipleSubscriptions),
    NL_TEST_DEF("TestReadHandler_SubscriptionAppRejection", TestReadInteraction::TestReadHandler_SubscriptionAppRejection),
    NL_TEST_DEF("TestReadHandler_MultipleSubscriptionsWithDataVersionFilter", TestReadInteraction::TestReadHandler_MultipleSubscriptionsWithDataVersionFilter),
    NL_TEST_DEF("TestReadHandler_MultipleReads", TestReadInteraction::TestReadHandler_MultipleReads),
    NL_TEST_DEF("TestReadHandler_OneSubscribeMultipleReads", TestReadInteraction::TestReadHandler_OneSubscribeMultipleReads),
    NL_TEST_DEF("TestReadHandler_TwoSubscribesMultipleReads", TestReadInteraction::TestReadHandler_TwoSubscribesMultipleReads),
    NL_TEST_DEF("TestReadHandlerResourceExhaustion_MultipleReads", TestReadInteraction::TestReadHandlerResourceExhaustion_MultipleReads),
    NL_TEST_DEF("TestReadAttributeTimeout", TestReadInteraction::TestReadAttributeTimeout),
    NL_TEST_DEF("TestSubscribeAttributeTimeout", TestReadInteraction::TestSubscribeAttributeTimeout),
    NL_TEST_DEF("TestReadHandler_SubscriptionAlteredReportingIntervals", TestReadInteraction::TestReadHandler_SubscriptionAlteredReportingIntervals),
    NL_TEST_DEF("TestReadSubscribeAttributeResponseWithCache", TestReadInteraction::TestReadSubscribeAttributeResponseWithCache),
    NL_TEST_DEF("TestReadHandler_KillOverQuotaSubscriptions", TestReadInteraction::TestReadHandler_KillOverQuotaSubscriptions),
    NL_TEST_DEF("TestReadHandler_KillOldestSubscriptions", TestReadInteraction::TestReadHandler_KillOldestSubscriptions),
    NL_TEST_DEF("TestReadHandler_TwoParallelReads", TestReadInteraction::TestReadHandler_TwoParallelReads),
    NL_TEST_DEF("TestReadHandler_TooManyPaths", TestReadInteraction::TestReadHandler_TooManyPaths),
    NL_TEST_DEF("TestReadHandler_TwoParallelReadsSecondTooManyPaths", TestReadInteraction::TestReadHandler_TwoParallelReadsSecondTooManyPaths),
    NL_TEST_SENTINEL()
};
// clang-format on

// clang-format off
nlTestSuite sSuite =
{
    "TestRead",
    &sTests[0],
    TestContext::Initialize,
    TestContext::Finalize
};
// clang-format on

} // namespace

int TestReadInteractionTest()
{
    TestContext gContext;
    nlTestRunner(&sSuite, &gContext);
    return (nlTestRunnerStats(&sSuite));
}

CHIP_REGISTER_TEST_SUITE(TestReadInteractionTest)
