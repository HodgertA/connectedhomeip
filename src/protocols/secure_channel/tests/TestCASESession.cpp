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

/**
 *    @file
 *      This file implements unit tests for the CASESession implementation.
 */

#include <credentials/CHIPCert.h>
#include <credentials/GroupDataProviderImpl.h>
#include <errno.h>
#include <lib/core/CHIPCore.h>
#include <lib/core/CHIPSafeCasts.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/ScopedBuffer.h>
#include <lib/support/TestPersistentStorageDelegate.h>
#include <lib/support/UnitTestRegistration.h>
#include <messaging/tests/MessagingContext.h>
#include <nlunit-test.h>
#include <protocols/secure_channel/CASEServer.h>
#include <protocols/secure_channel/CASESession.h>
#include <stdarg.h>

#include "credentials/tests/CHIPCert_test_vectors.h"

using namespace chip;
using namespace Credentials;
using namespace TestCerts;

using namespace chip;
using namespace chip::Inet;
using namespace chip::Transport;
using namespace chip::Messaging;
using namespace chip::Protocols;

using TestContext = Test::LoopbackMessagingContext;

namespace {
#if CHIP_CONFIG_SLOW_CRYPTO
constexpr uint32_t sTestCaseMessageCount           = 8;
constexpr uint32_t sTestCaseResumptionMessageCount = 6;
#else  // CHIP_CONFIG_SLOW_CRYPTO
constexpr uint32_t sTestCaseMessageCount           = 5;
constexpr uint32_t sTestCaseResumptionMessageCount = 4;
#endif // CHIP_CONFIG_SLOW_CRYPTO

FabricTable gCommissionerFabrics;
FabricIndex gCommissionerFabricIndex;
GroupDataProviderImpl gCommissionerGroupDataProvider;
TestPersistentStorageDelegate gCommissionerStorageDelegate;

FabricTable gDeviceFabrics;
FabricIndex gDeviceFabricIndex;
GroupDataProviderImpl gDeviceGroupDataProvider;
TestPersistentStorageDelegate gDeviceStorageDelegate;

NodeId Node01_01 = 0xDEDEDEDE00010001;
NodeId Node01_02 = 0xDEDEDEDE00010002;

class TestCASESecurePairingDelegate : public SessionEstablishmentDelegate
{
public:
    void OnSessionEstablishmentError(CHIP_ERROR error) override { mNumPairingErrors++; }

    void OnSessionEstablished(const SessionHandle & session) override
    {
        mSession.Grab(session);
        mNumPairingComplete++;
    }

    SessionHolder & GetSessionHolder() { return mSession; }

    SessionHolder mSession;

    // TODO: Rename mNumPairing* to mNumEstablishment*
    uint32_t mNumPairingErrors   = 0;
    uint32_t mNumPairingComplete = 0;
};

class CASEServerForTest : public CASEServer
{
public:
    CASESession & GetSession() override { return mCaseSession; }

private:
    CASESession mCaseSession;
};

CHIP_ERROR InitTestIpk(GroupDataProvider & groupDataProvider, const FabricInfo & fabricInfo, size_t numIpks)
{
    VerifyOrReturnError((numIpks > 0) && (numIpks <= 3), CHIP_ERROR_INVALID_ARGUMENT);
    using KeySet         = chip::Credentials::GroupDataProvider::KeySet;
    using SecurityPolicy = chip::Credentials::GroupDataProvider::SecurityPolicy;

    KeySet ipkKeySet(GroupDataProvider::kIdentityProtectionKeySetId, SecurityPolicy::kTrustFirst, static_cast<uint8_t>(numIpks));

    for (size_t ipkIndex = 0; ipkIndex < numIpks; ++ipkIndex)
    {
        // Set start time to 0, 1000, 2000, etc
        ipkKeySet.epoch_keys[ipkIndex].start_time = static_cast<uint64_t>(ipkIndex * 1000);
        // Set IPK Epoch key to 00.....00, 01....01, 02.....02, etc
        memset(&ipkKeySet.epoch_keys[ipkIndex].key, static_cast<int>(ipkIndex), sizeof(ipkKeySet.epoch_keys[ipkIndex].key));
    }

    uint8_t compressedId[sizeof(uint64_t)];
    MutableByteSpan compressedIdSpan(compressedId);
    ReturnErrorOnFailure(fabricInfo.GetCompressedId(compressedIdSpan));
    return groupDataProvider.SetKeySet(fabricInfo.GetFabricIndex(), compressedIdSpan, ipkKeySet);
}

CHIP_ERROR InitCredentialSets()
{
    gCommissionerStorageDelegate.ClearStorage();
    gCommissionerGroupDataProvider.SetStorageDelegate(&gCommissionerStorageDelegate);
    ReturnErrorOnFailure(gCommissionerGroupDataProvider.Init());

    FabricInfo commissionerFabric;

    P256SerializedKeypair opKeysSerialized;
    // TODO: Rename gCommissioner* to gInitiator*
    memcpy((uint8_t *) (opKeysSerialized), sTestCert_Node01_02_PublicKey, sTestCert_Node01_02_PublicKey_Len);
    memcpy((uint8_t *) (opKeysSerialized) + sTestCert_Node01_02_PublicKey_Len, sTestCert_Node01_02_PrivateKey,
           sTestCert_Node01_02_PrivateKey_Len);

    ReturnErrorOnFailure(opKeysSerialized.SetLength(sTestCert_Node01_02_PublicKey_Len + sTestCert_Node01_02_PrivateKey_Len));

    P256Keypair opKey;
    ReturnErrorOnFailure(opKey.Deserialize(opKeysSerialized));
    ReturnErrorOnFailure(commissionerFabric.SetOperationalKeypair(&opKey));

    ReturnErrorOnFailure(commissionerFabric.SetRootCert(ByteSpan(sTestCert_Root01_Chip, sTestCert_Root01_Chip_Len)));
    ReturnErrorOnFailure(commissionerFabric.SetICACert(ByteSpan(sTestCert_ICA01_Chip, sTestCert_ICA01_Chip_Len)));
    ReturnErrorOnFailure(commissionerFabric.SetNOCCert(ByteSpan(sTestCert_Node01_02_Chip, sTestCert_Node01_02_Chip_Len)));

    ReturnErrorOnFailure(gCommissionerFabrics.AddNewFabric(commissionerFabric, &gCommissionerFabricIndex));

    FabricInfo * newFabric = gCommissionerFabrics.FindFabricWithIndex(gCommissionerFabricIndex);
    VerifyOrReturnError(newFabric != nullptr, CHIP_ERROR_INTERNAL);
    ReturnErrorOnFailure(InitTestIpk(gCommissionerGroupDataProvider, *newFabric, /* numIpks= */ 1));

    gDeviceStorageDelegate.ClearStorage();
    gDeviceGroupDataProvider.SetStorageDelegate(&gDeviceStorageDelegate);
    ReturnErrorOnFailure(gDeviceGroupDataProvider.Init());
    FabricInfo deviceFabric;

    memcpy((uint8_t *) (opKeysSerialized), sTestCert_Node01_01_PublicKey, sTestCert_Node01_01_PublicKey_Len);
    memcpy((uint8_t *) (opKeysSerialized) + sTestCert_Node01_01_PublicKey_Len, sTestCert_Node01_01_PrivateKey,
           sTestCert_Node01_01_PrivateKey_Len);

    ReturnErrorOnFailure(opKeysSerialized.SetLength(sTestCert_Node01_01_PublicKey_Len + sTestCert_Node01_01_PrivateKey_Len));

    ReturnErrorOnFailure(opKey.Deserialize(opKeysSerialized));
    ReturnErrorOnFailure(deviceFabric.SetOperationalKeypair(&opKey));

    ReturnErrorOnFailure(deviceFabric.SetRootCert(ByteSpan(sTestCert_Root01_Chip, sTestCert_Root01_Chip_Len)));
    ReturnErrorOnFailure(deviceFabric.SetICACert(ByteSpan(sTestCert_ICA01_Chip, sTestCert_ICA01_Chip_Len)));
    ReturnErrorOnFailure(deviceFabric.SetNOCCert(ByteSpan(sTestCert_Node01_01_Chip, sTestCert_Node01_01_Chip_Len)));

    ReturnErrorOnFailure(gDeviceFabrics.AddNewFabric(deviceFabric, &gDeviceFabricIndex));

    // TODO: Validate more cases of number of IPKs on both sides
    newFabric = gDeviceFabrics.FindFabricWithIndex(gDeviceFabricIndex);
    VerifyOrReturnError(newFabric != nullptr, CHIP_ERROR_INTERNAL);
    ReturnErrorOnFailure(InitTestIpk(gDeviceGroupDataProvider, *newFabric, /* numIpks= */ 1));

    return CHIP_NO_ERROR;
}

} // namespace

void CASE_SecurePairingWaitTest(nlTestSuite * inSuite, void * inContext)
{
    SessionManager sessionManager;

    // Test all combinations of invalid parameters
    TestCASESecurePairingDelegate delegate;
    CASESession pairing;
    FabricTable fabrics;

    NL_TEST_ASSERT(inSuite, pairing.GetSecureSessionType() == SecureSession::Type::kCASE);

    pairing.SetGroupDataProvider(&gDeviceGroupDataProvider);
    NL_TEST_ASSERT(inSuite,
                   pairing.ListenForSessionEstablishment(sessionManager, nullptr, nullptr, nullptr, nullptr) ==
                       CHIP_ERROR_INVALID_ARGUMENT);
    NL_TEST_ASSERT(inSuite,
                   pairing.ListenForSessionEstablishment(sessionManager, nullptr, nullptr, nullptr, &delegate) ==
                       CHIP_ERROR_INVALID_ARGUMENT);
    NL_TEST_ASSERT(inSuite,
                   pairing.ListenForSessionEstablishment(sessionManager, &fabrics, nullptr, nullptr, &delegate) == CHIP_NO_ERROR);
}

void CASE_SecurePairingStartTest(nlTestSuite * inSuite, void * inContext)
{
    TestContext & ctx = *reinterpret_cast<TestContext *>(inContext);
    SessionManager sessionManager;

    // Test all combinations of invalid parameters
    TestCASESecurePairingDelegate delegate;
    CASESession pairing;
    pairing.SetGroupDataProvider(&gCommissionerGroupDataProvider);

    ExchangeContext * context = ctx.NewUnauthenticatedExchangeToBob(&pairing);

    NL_TEST_ASSERT(inSuite,
                   pairing.EstablishSession(sessionManager, nullptr, gCommissionerFabricIndex, Node01_01, nullptr, nullptr, nullptr,
                                            nullptr) != CHIP_NO_ERROR);
    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(inSuite,
                   pairing.EstablishSession(sessionManager, &gCommissionerFabrics, gCommissionerFabricIndex, Node01_01, nullptr,
                                            nullptr, nullptr, nullptr) != CHIP_NO_ERROR);
    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(inSuite,
                   pairing.EstablishSession(sessionManager, &gCommissionerFabrics, gCommissionerFabricIndex, Node01_01, context,
                                            nullptr, nullptr, &delegate) == CHIP_NO_ERROR);
    ctx.DrainAndServiceIO();

    auto & loopback = ctx.GetLoopback();
    NL_TEST_ASSERT(inSuite, loopback.mSentMessageCount == 1);

    // Clear pending packet in CRMP
    ReliableMessageMgr * rm     = ctx.GetExchangeManager().GetReliableMessageMgr();
    ReliableMessageContext * rc = context->GetReliableMessageContext();
    rm->ClearRetransTable(rc);

    loopback.mMessageSendError = CHIP_ERROR_BAD_REQUEST;

    CASESession pairing1;
    pairing1.SetGroupDataProvider(&gCommissionerGroupDataProvider);

    loopback.mSentMessageCount = 0;
    loopback.mMessageSendError = CHIP_ERROR_BAD_REQUEST;
    ExchangeContext * context1 = ctx.NewUnauthenticatedExchangeToBob(&pairing1);

    NL_TEST_ASSERT(inSuite,
                   pairing1.EstablishSession(sessionManager, &gCommissionerFabrics, gCommissionerFabricIndex, Node01_01, context1,
                                             nullptr, nullptr, &delegate) == CHIP_ERROR_BAD_REQUEST);
    ctx.DrainAndServiceIO();

    loopback.mMessageSendError = CHIP_NO_ERROR;
}

void CASE_SecurePairingHandshakeTestCommon(nlTestSuite * inSuite, void * inContext, SessionManager & sessionManager,
                                           CASESession & pairingCommissioner, TestCASESecurePairingDelegate & delegateCommissioner)
{
    TestContext & ctx = *reinterpret_cast<TestContext *>(inContext);

    // Test all combinations of invalid parameters
    TestCASESecurePairingDelegate delegateAccessory;
    CASESession pairingAccessory;
    ReliableMessageProtocolConfig verySleepyAccessoryRmpConfig(System::Clock::Milliseconds32(360000),
                                                               System::Clock::Milliseconds32(100000));
    ReliableMessageProtocolConfig nonSleepyCommissionerRmpConfig(System::Clock::Milliseconds32(5000),
                                                                 System::Clock::Milliseconds32(300));

    auto & loopback            = ctx.GetLoopback();
    loopback.mSentMessageCount = 0;

    NL_TEST_ASSERT(inSuite,
                   ctx.GetExchangeManager().RegisterUnsolicitedMessageHandlerForType(Protocols::SecureChannel::MsgType::CASE_Sigma1,
                                                                                     &pairingAccessory) == CHIP_NO_ERROR);

    ExchangeContext * contextCommissioner = ctx.NewUnauthenticatedExchangeToBob(&pairingCommissioner);

    pairingAccessory.SetGroupDataProvider(&gDeviceGroupDataProvider);
    NL_TEST_ASSERT(inSuite,
                   pairingAccessory.ListenForSessionEstablishment(sessionManager, &gDeviceFabrics, nullptr, nullptr,
                                                                  &delegateAccessory,
                                                                  MakeOptional(verySleepyAccessoryRmpConfig)) == CHIP_NO_ERROR);
    NL_TEST_ASSERT(inSuite,
                   pairingCommissioner.EstablishSession(sessionManager, &gCommissionerFabrics, gCommissionerFabricIndex, Node01_01,
                                                        contextCommissioner, nullptr, nullptr, &delegateCommissioner,
                                                        MakeOptional(nonSleepyCommissionerRmpConfig)) == CHIP_NO_ERROR);
    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(inSuite, loopback.mSentMessageCount == sTestCaseMessageCount);
    NL_TEST_ASSERT(inSuite, delegateAccessory.mNumPairingComplete == 1);
    NL_TEST_ASSERT(inSuite, delegateCommissioner.mNumPairingComplete == 1);
    NL_TEST_ASSERT(inSuite, pairingAccessory.GetRemoteMRPConfig().mIdleRetransTimeout == System::Clock::Milliseconds32(5000));
    NL_TEST_ASSERT(inSuite, pairingAccessory.GetRemoteMRPConfig().mActiveRetransTimeout == System::Clock::Milliseconds32(300));
    NL_TEST_ASSERT(inSuite, pairingCommissioner.GetRemoteMRPConfig().mIdleRetransTimeout == System::Clock::Milliseconds32(360000));
    NL_TEST_ASSERT(inSuite,
                   pairingCommissioner.GetRemoteMRPConfig().mActiveRetransTimeout == System::Clock::Milliseconds32(100000));
}

void CASE_SecurePairingHandshakeTest(nlTestSuite * inSuite, void * inContext)
{
    SessionManager sessionManager;
    TestCASESecurePairingDelegate delegateCommissioner;
    CASESession pairingCommissioner;
    pairingCommissioner.SetGroupDataProvider(&gCommissionerGroupDataProvider);
    CASE_SecurePairingHandshakeTestCommon(inSuite, inContext, sessionManager, pairingCommissioner, delegateCommissioner);
}

CASEServerForTest gPairingServer;

void CASE_SecurePairingHandshakeServerTest(nlTestSuite * inSuite, void * inContext)
{
    // TODO: Add cases for mismatching IPK config between initiator/responder

    TestCASESecurePairingDelegate delegateCommissioner;

    auto * pairingCommissioner = chip::Platform::New<CASESession>();
    pairingCommissioner->SetGroupDataProvider(&gCommissionerGroupDataProvider);

    TestContext & ctx = *reinterpret_cast<TestContext *>(inContext);

    auto & loopback            = ctx.GetLoopback();
    loopback.mSentMessageCount = 0;

    // Use the same session manager on both CASE client and server sides to validate that both
    // components may work simultaneously on a single device.
    NL_TEST_ASSERT(inSuite,
                   gPairingServer.ListenForSessionEstablishment(&ctx.GetExchangeManager(), &ctx.GetSecureSessionManager(),
                                                                &gDeviceFabrics, nullptr, nullptr,
                                                                &gDeviceGroupDataProvider) == CHIP_NO_ERROR);

    ExchangeContext * contextCommissioner = ctx.NewUnauthenticatedExchangeToBob(pairingCommissioner);

    FabricInfo * fabric = gCommissionerFabrics.FindFabricWithIndex(gCommissionerFabricIndex);
    NL_TEST_ASSERT(inSuite, fabric != nullptr);

    NL_TEST_ASSERT(inSuite,
                   pairingCommissioner->EstablishSession(ctx.GetSecureSessionManager(), &gCommissionerFabrics,
                                                         gCommissionerFabricIndex, Node01_01, contextCommissioner, nullptr, nullptr,
                                                         &delegateCommissioner) == CHIP_NO_ERROR);
    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(inSuite, loopback.mSentMessageCount == sTestCaseMessageCount);
    NL_TEST_ASSERT(inSuite, delegateCommissioner.mNumPairingComplete == 1);

    // Validate that secure session is created
    SessionHolder & holder = delegateCommissioner.GetSessionHolder();
    NL_TEST_ASSERT(inSuite, bool(holder));
    NL_TEST_ASSERT(inSuite, holder->GetPeer() == fabric->GetScopedNodeIdForNode(Node01_01));

    auto * pairingCommissioner1 = chip::Platform::New<CASESession>();
    pairingCommissioner1->SetGroupDataProvider(&gCommissionerGroupDataProvider);
    ExchangeContext * contextCommissioner1 = ctx.NewUnauthenticatedExchangeToBob(pairingCommissioner1);

    NL_TEST_ASSERT(inSuite,
                   pairingCommissioner1->EstablishSession(ctx.GetSecureSessionManager(), &gCommissionerFabrics,
                                                          gCommissionerFabricIndex, Node01_01, contextCommissioner1, nullptr,
                                                          nullptr, &delegateCommissioner) == CHIP_NO_ERROR);
    ctx.DrainAndServiceIO();

    chip::Platform::Delete(pairingCommissioner);
    chip::Platform::Delete(pairingCommissioner1);
}

struct Sigma1Params
{
    // Purposefully not using constants like kSigmaParamRandomNumberSize that
    // the code uses, so we have a cross-check.
    static constexpr size_t initiatorRandomLen    = 32;
    static constexpr uint16_t initiatorSessionId  = 0;
    static constexpr size_t destinationIdLen      = 32;
    static constexpr size_t initiatorEphPubKeyLen = 65;
    static constexpr size_t resumptionIdLen       = 0; // Nonzero means include it.
    static constexpr size_t initiatorResumeMICLen = 0; // Nonzero means include it.

    static constexpr uint8_t initiatorRandomTag    = 1;
    static constexpr uint8_t initiatorSessionIdTag = 2;
    static constexpr uint8_t destinationIdTag      = 3;
    static constexpr uint8_t initiatorEphPubKeyTag = 4;
    static constexpr uint8_t resumptionIdTag       = 6;
    static constexpr uint8_t initiatorResumeMICTag = 7;
    static constexpr TLV::Tag NumToTag(uint8_t num) { return TLV::ContextTag(num); }

    static constexpr bool includeStructEnd = true;

    static constexpr bool expectSuccess = true;
};

void CASE_DestinationIdTest(nlTestSuite * inSuite, void * inContext)
{
    // Validate example test vector from CASE section of spec

    const uint8_t kRootPubKeyFromSpec[Crypto::CHIP_CRYPTO_PUBLIC_KEY_SIZE_BYTES] = {
        0x04, 0x4a, 0x9f, 0x42, 0xb1, 0xca, 0x48, 0x40, 0xd3, 0x72, 0x92, 0xbb, 0xc7, 0xf6, 0xa7, 0xe1, 0x1e,
        0x22, 0x20, 0x0c, 0x97, 0x6f, 0xc9, 0x00, 0xdb, 0xc9, 0x8a, 0x7a, 0x38, 0x3a, 0x64, 0x1c, 0xb8, 0x25,
        0x4a, 0x2e, 0x56, 0xd4, 0xe2, 0x95, 0xa8, 0x47, 0x94, 0x3b, 0x4e, 0x38, 0x97, 0xc4, 0xa7, 0x73, 0xe9,
        0x30, 0x27, 0x7b, 0x4d, 0x9f, 0xbe, 0xde, 0x8a, 0x05, 0x26, 0x86, 0xbf, 0xac, 0xfa
    };

    const uint8_t kIpkOperationalGroupKeyFromSpec[Crypto::CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES] = {
        0x9b, 0xc6, 0x1c, 0xd9, 0xc6, 0x2a, 0x2d, 0xf6, 0xd6, 0x4d, 0xfc, 0xaa, 0x9d, 0xc4, 0x72, 0xd4
    };

    const uint8_t kInitiatorRandomFromSpec[Sigma1Params::initiatorRandomLen] = { 0x7e, 0x17, 0x12, 0x31, 0x56, 0x8d, 0xfa, 0x17,
                                                                                 0x20, 0x6b, 0x3a, 0xcc, 0xf8, 0xfa, 0xec, 0x2f,
                                                                                 0x4d, 0x21, 0xb5, 0x80, 0x11, 0x31, 0x96, 0xf4,
                                                                                 0x7c, 0x7c, 0x4d, 0xeb, 0x81, 0x0a, 0x73, 0xdc };

    const uint8_t kExpectedDestinationIdFromSpec[Crypto::kSHA256_Hash_Length] = { 0xdc, 0x35, 0xdd, 0x5f, 0xc9, 0x13, 0x4c, 0xc5,
                                                                                  0x54, 0x45, 0x38, 0xc9, 0xc3, 0xfc, 0x42, 0x97,
                                                                                  0xc1, 0xec, 0x33, 0x70, 0xc8, 0x39, 0x13, 0x6a,
                                                                                  0x80, 0xe1, 0x07, 0x96, 0x45, 0x1d, 0x4c, 0x53 };

    const FabricId kFabricIdFromSpec = 0x2906C908D115D362;
    const NodeId kNodeIdFromSpec     = 0xCD5544AA7B13EF14;

    uint8_t destinationIdBuf[Crypto::kSHA256_Hash_Length] = { 0 };
    MutableByteSpan destinationIdSpan(destinationIdBuf);

    // Test exact example
    CHIP_ERROR err =
        GenerateCaseDestinationId(ByteSpan(kIpkOperationalGroupKeyFromSpec), ByteSpan(kInitiatorRandomFromSpec),
                                  ByteSpan(kRootPubKeyFromSpec), kFabricIdFromSpec, kNodeIdFromSpec, destinationIdSpan);
    NL_TEST_ASSERT(inSuite, CHIP_NO_ERROR == err);
    NL_TEST_ASSERT(inSuite, destinationIdSpan.size() == sizeof(destinationIdBuf));
    NL_TEST_ASSERT(inSuite, destinationIdSpan.data_equal(ByteSpan(kExpectedDestinationIdFromSpec)));

    memset(destinationIdSpan.data(), 0, destinationIdSpan.size());

    // Test changing input: should yield different
    err = GenerateCaseDestinationId(ByteSpan(kIpkOperationalGroupKeyFromSpec), ByteSpan(kInitiatorRandomFromSpec),
                                    ByteSpan(kRootPubKeyFromSpec), kFabricIdFromSpec,
                                    kNodeIdFromSpec + 1, // <--- Change node ID
                                    destinationIdSpan);
    NL_TEST_ASSERT(inSuite, CHIP_NO_ERROR == err);
    NL_TEST_ASSERT(inSuite, destinationIdSpan.size() == sizeof(destinationIdBuf));
    NL_TEST_ASSERT(inSuite, !destinationIdSpan.data_equal(ByteSpan(kExpectedDestinationIdFromSpec)));
}

template <typename Params>
static CHIP_ERROR EncodeSigma1(MutableByteSpan & buf)
{
    using namespace TLV;

    TLVWriter writer;
    writer.Init(buf);

    TLVType containerType;
    ReturnErrorOnFailure(writer.StartContainer(AnonymousTag(), kTLVType_Structure, containerType));
    uint8_t initiatorRandom[Params::initiatorRandomLen] = { 1 };
    ReturnErrorOnFailure(writer.Put(Params::NumToTag(Params::initiatorRandomTag), ByteSpan(initiatorRandom)));

    ReturnErrorOnFailure(writer.Put(Params::NumToTag(Params::initiatorSessionIdTag), Params::initiatorSessionId));

    uint8_t destinationId[Params::destinationIdLen] = { 2 };
    ReturnErrorOnFailure(writer.Put(Params::NumToTag(Params::destinationIdTag), ByteSpan(destinationId)));

    uint8_t initiatorEphPubKey[Params::initiatorEphPubKeyLen] = { 3 };
    ReturnErrorOnFailure(writer.Put(Params::NumToTag(Params::initiatorEphPubKeyTag), ByteSpan(initiatorEphPubKey)));

    // I wish we had "if constexpr" support here, so the compiler would know
    // resumptionIdLen is nonzero inside the block....
    if (Params::resumptionIdLen != 0)
    {
        uint8_t resumptionId[Params::resumptionIdLen];

        // to fix _FORTIFY_SOURCE issue, _FORTIFY_SOURCE=2 by default on Android
        (&memset)(resumptionId, 4, Params::resumptionIdLen);
        ReturnErrorOnFailure(
            writer.Put(Params::NumToTag(Params::resumptionIdTag), ByteSpan(resumptionId, Params::resumptionIdLen)));
    }

    if (Params::initiatorResumeMICLen != 0)
    {
        uint8_t initiatorResumeMIC[Params::initiatorResumeMICLen];
        // to fix _FORTIFY_SOURCE issue, _FORTIFY_SOURCE=2 by default on Android
        (&memset)(initiatorResumeMIC, 5, Params::initiatorResumeMICLen);
        ReturnErrorOnFailure(writer.Put(Params::NumToTag(Params::initiatorResumeMICTag),
                                        ByteSpan(initiatorResumeMIC, Params::initiatorResumeMICLen)));
    }

    if (Params::includeStructEnd)
    {
        ReturnErrorOnFailure(writer.EndContainer(containerType));
    }

    buf.reduce_size(writer.GetLengthWritten());
    return CHIP_NO_ERROR;
}

// A macro, so we can tell which test failed based on line number.
#define TestSigma1Parsing(inSuite, mem, bufferSize, params)                                                                        \
    do                                                                                                                             \
    {                                                                                                                              \
        MutableByteSpan buf(mem.Get(), bufferSize);                                                                                \
        CHIP_ERROR err = EncodeSigma1<params>(buf);                                                                                \
        NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);                                                                             \
                                                                                                                                   \
        TLV::ContiguousBufferTLVReader reader;                                                                                     \
        reader.Init(buf);                                                                                                          \
                                                                                                                                   \
        ByteSpan initiatorRandom;                                                                                                  \
        uint16_t initiatorSessionId;                                                                                               \
        ByteSpan destinationId;                                                                                                    \
        ByteSpan initiatorEphPubKey;                                                                                               \
        bool resumptionRequested;                                                                                                  \
        ByteSpan resumptionId;                                                                                                     \
        ByteSpan initiatorResumeMIC;                                                                                               \
        CASESession session;                                                                                                       \
        err = session.ParseSigma1(reader, initiatorRandom, initiatorSessionId, destinationId, initiatorEphPubKey,                  \
                                  resumptionRequested, resumptionId, initiatorResumeMIC);                                          \
        NL_TEST_ASSERT(inSuite, (err == CHIP_NO_ERROR) == params::expectSuccess);                                                  \
        if (params::expectSuccess)                                                                                                 \
        {                                                                                                                          \
            NL_TEST_ASSERT(inSuite, resumptionRequested == (params::resumptionIdLen != 0 && params::initiatorResumeMICLen != 0));  \
            /* Add other verification tests here as desired */                                                                     \
        }                                                                                                                          \
    } while (0)

struct BadSigma1ParamsBase : public Sigma1Params
{
    static constexpr bool expectSuccess = false;
};

struct Sigma1NoStructEnd : public BadSigma1ParamsBase
{
    static constexpr bool includeStructEnd = false;
};

struct Sigma1WrongTags : public BadSigma1ParamsBase
{
    static constexpr TLV::Tag NumToTag(uint8_t num) { return TLV::ProfileTag(0, num); }
};

struct Sigma1TooLongRandom : public BadSigma1ParamsBase
{
    static constexpr size_t initiatorRandomLen = 33;
};

struct Sigma1TooShortRandom : public BadSigma1ParamsBase
{
    static constexpr size_t initiatorRandomLen = 31;
};

struct Sigma1TooLongDest : public BadSigma1ParamsBase
{
    static constexpr size_t destinationIdLen = 33;
};

struct Sigma1TooShortDest : public BadSigma1ParamsBase
{
    static constexpr size_t destinationIdLen = 31;
};

struct Sigma1TooLongPubkey : public BadSigma1ParamsBase
{
    static constexpr size_t initiatorEphPubKeyLen = 66;
};

struct Sigma1TooShortPubkey : public BadSigma1ParamsBase
{
    static constexpr size_t initiatorEphPubKeyLen = 64;
};

struct Sigma1WithResumption : public Sigma1Params
{
    static constexpr size_t resumptionIdLen       = 16;
    static constexpr size_t initiatorResumeMICLen = 16;
};

struct Sigma1TooLongResumptionId : public Sigma1WithResumption
{
    static constexpr size_t resumptionIdLen = 17;
    static constexpr bool expectSuccess     = false;
};

struct Sigma1TooShortResumptionId : public BadSigma1ParamsBase
{
    static constexpr size_t resumptionIdLen = 15;
    static constexpr bool expectSuccess     = false;
};

struct Sigma1TooLongResumeMIC : public Sigma1WithResumption
{
    static constexpr size_t resumptionIdLen = 17;
    static constexpr bool expectSuccess     = false;
};

struct Sigma1TooShortResumeMIC : public Sigma1WithResumption
{
    static constexpr size_t initiatorResumeMICLen = 15;
    static constexpr bool expectSuccess           = false;
};

struct Sigma1SessionIdMax : public Sigma1Params
{
    static constexpr uint32_t initiatorSessionId = UINT16_MAX;
};

struct Sigma1SessionIdTooBig : public BadSigma1ParamsBase
{
    static constexpr uint32_t initiatorSessionId = UINT16_MAX + 1;
};

static void CASE_Sigma1ParsingTest(nlTestSuite * inSuite, void * inContext)
{
    // 1280 bytes must be enough by definition.
    constexpr size_t bufferSize = 1280;
    chip::Platform::ScopedMemoryBuffer<uint8_t> mem;
    NL_TEST_ASSERT(inSuite, mem.Calloc(bufferSize));

    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1Params);

    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1NoStructEnd);
    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1WrongTags);
    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1TooLongRandom);
    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1TooShortRandom);
    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1TooLongDest);
    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1TooShortDest);
    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1TooLongPubkey);
    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1TooShortPubkey);
    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1WithResumption);
    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1TooLongResumptionId);
    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1TooShortResumptionId);
    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1TooLongResumeMIC);
    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1TooShortResumeMIC);
    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1SessionIdMax);
    TestSigma1Parsing(inSuite, mem, bufferSize, Sigma1SessionIdTooBig);
}

struct SessionResumptionTestStorage : SessionResumptionStorage
{
    SessionResumptionTestStorage(CHIP_ERROR findMethodReturnCode, ScopedNodeId peerNodeId, ResumptionIdStorage * resumptionId,
                                 Crypto::P256ECDHDerivedSecret * sharedSecret) :
        mFindMethodReturnCode(findMethodReturnCode),
        mPeerNodeId(peerNodeId), mResumptionId(resumptionId), mSharedSecret(sharedSecret)
    {}
    SessionResumptionTestStorage(CHIP_ERROR findMethodReturnCode) : mFindMethodReturnCode(findMethodReturnCode) {}
    CHIP_ERROR FindByScopedNodeId(const ScopedNodeId & node, ResumptionIdStorage & resumptionId,
                                  Crypto::P256ECDHDerivedSecret & sharedSecret, CATValues & peerCATs) override
    {
        if (mResumptionId != nullptr)
        {
            memcpy(resumptionId.data(), mResumptionId->data(), mResumptionId->size());
        }
        if (mSharedSecret != nullptr)
        {
            memcpy(sharedSecret.Bytes(), mSharedSecret->Bytes(), mSharedSecret->Length());
            sharedSecret.SetLength(mSharedSecret->Length());
        }
        peerCATs = CATValues{};
        return mFindMethodReturnCode;
    }
    CHIP_ERROR FindByResumptionId(ConstResumptionIdView resumptionId, ScopedNodeId & node,
                                  Crypto::P256ECDHDerivedSecret & sharedSecret, CATValues & peerCATs) override
    {
        node = mPeerNodeId;
        if (mSharedSecret != nullptr)
        {
            memcpy(sharedSecret.Bytes(), mSharedSecret->Bytes(), mSharedSecret->Length());
            sharedSecret.SetLength(mSharedSecret->Length());
        }
        peerCATs = CATValues{};
        return mFindMethodReturnCode;
    }
    CHIP_ERROR Save(const ScopedNodeId & node, ConstResumptionIdView resumptionId,
                    const Crypto::P256ECDHDerivedSecret & sharedSecret, const CATValues & peerCATs) override
    {
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR DeleteAll(const FabricIndex fabricIndex) override { return CHIP_NO_ERROR; }
    CHIP_ERROR mFindMethodReturnCode;
    ScopedNodeId mPeerNodeId;
    ResumptionIdStorage * mResumptionId           = nullptr;
    Crypto::P256ECDHDerivedSecret * mSharedSecret = nullptr;
};

static void CASE_SessionResumptionStorage(nlTestSuite * inSuite, void * inContext)
{
    // Test the SessionResumptionStorage external interface.
    //
    // Our build should accept any storage delegate injected that implements
    // this.  And if our delegate provides usable session resumption
    // information, session resumption should succeed.  In the case that the
    // delegate cannot provide the information needed for session resumption, or
    // if the peers have mismatched session resumption information, we should
    // fall back to CASE.

    TestContext & ctx = *reinterpret_cast<TestContext *>(inContext);
    TestCASESecurePairingDelegate delegateCommissioner;
    chip::SessionResumptionStorage::ResumptionIdStorage resumptionIdA;
    chip::SessionResumptionStorage::ResumptionIdStorage resumptionIdB;
    chip::Crypto::P256ECDHDerivedSecret sharedSecretA;
    chip::Crypto::P256ECDHDerivedSecret sharedSecretB;

    // Create our fabric-scoped node IDs.
    FabricInfo * fabric = gCommissionerFabrics.FindFabricWithIndex(gCommissionerFabricIndex);
    NL_TEST_ASSERT(inSuite, fabric != nullptr);
    ScopedNodeId initiator = fabric->GetScopedNodeIdForNode(Node01_02);
    ScopedNodeId responder = fabric->GetScopedNodeIdForNode(Node01_01);

    // Generate a resumption IDs.
    NL_TEST_ASSERT(inSuite, CHIP_NO_ERROR == chip::Crypto::DRBG_get_bytes(resumptionIdA.data(), resumptionIdA.size()));
    NL_TEST_ASSERT(inSuite, CHIP_NO_ERROR == chip::Crypto::DRBG_get_bytes(resumptionIdB.data(), resumptionIdB.size()));

    // Generate a shared secrets.
    sharedSecretA.SetLength(sharedSecretA.Capacity());
    NL_TEST_ASSERT(inSuite, CHIP_NO_ERROR == chip::Crypto::DRBG_get_bytes(sharedSecretA.Bytes(), sharedSecretA.Length()));
    sharedSecretB.SetLength(sharedSecretB.Capacity());
    NL_TEST_ASSERT(inSuite, CHIP_NO_ERROR == chip::Crypto::DRBG_get_bytes(sharedSecretB.Bytes(), sharedSecretB.Length()));

    struct
    {
        SessionResumptionTestStorage initiatorStorage;
        SessionResumptionTestStorage responderStorage;
        uint32_t expectedSentMessageCount;
    } testVectors[] = {
        // Both peers have a matching session resumption record.
        // This should succeed.
        {
            .initiatorStorage = SessionResumptionTestStorage(CHIP_NO_ERROR, responder, &resumptionIdA, &sharedSecretA),
            .responderStorage = SessionResumptionTestStorage(CHIP_NO_ERROR, initiator, &resumptionIdA, &sharedSecretA),
            .expectedSentMessageCount =
                sTestCaseResumptionMessageCount, // we expect this number of sent messages with successful session resumption
        },
        // Peers have mismatched session resumption records.
        // This should succeed with fall back to CASE.
        {
            .initiatorStorage         = SessionResumptionTestStorage(CHIP_NO_ERROR, responder, &resumptionIdA, &sharedSecretA),
            .responderStorage         = SessionResumptionTestStorage(CHIP_ERROR_KEY_NOT_FOUND),
            .expectedSentMessageCount = sTestCaseMessageCount, // we expect this number of sent message when we fall back to CASE
        },
        // Peers both have record of the same resumption ID, but a different shared secret.
        // This should succeed with fall back to CASE.
        {
            .initiatorStorage         = SessionResumptionTestStorage(CHIP_NO_ERROR, responder, &resumptionIdA, &sharedSecretA),
            .responderStorage         = SessionResumptionTestStorage(CHIP_NO_ERROR, initiator, &resumptionIdA, &sharedSecretB),
            .expectedSentMessageCount = sTestCaseMessageCount, // we expect this number of sent message when we fall back to CASE
        },
        // Neither peer has a session resumption record.
        // This should succeed - no attempt at session resumption will be made.
        {
            .initiatorStorage = SessionResumptionTestStorage(CHIP_ERROR_KEY_NOT_FOUND),
            .responderStorage = SessionResumptionTestStorage(CHIP_ERROR_KEY_NOT_FOUND),
            .expectedSentMessageCount =
                sTestCaseMessageCount, // we expect this number of sent messages if we do not attempt session resumption
        },
    };

    auto & loopback = ctx.GetLoopback();
    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); ++i)
    {
        auto * pairingCommissioner = chip::Platform::New<CASESession>();
        pairingCommissioner->SetGroupDataProvider(&gCommissionerGroupDataProvider);
        loopback.mSentMessageCount = 0;
        NL_TEST_ASSERT(inSuite,
                       gPairingServer.ListenForSessionEstablishment(&ctx.GetExchangeManager(), &ctx.GetSecureSessionManager(),
                                                                    &gDeviceFabrics, &testVectors[i].responderStorage, nullptr,
                                                                    &gDeviceGroupDataProvider) == CHIP_NO_ERROR);
        ExchangeContext * contextCommissioner = ctx.NewUnauthenticatedExchangeToBob(pairingCommissioner);
        auto establishmentReturnVal           = pairingCommissioner->EstablishSession(
            ctx.GetSecureSessionManager(), &gCommissionerFabrics, gCommissionerFabricIndex, Node01_01, contextCommissioner,
            &testVectors[i].initiatorStorage, nullptr, &delegateCommissioner);
        ctx.DrainAndServiceIO();
        NL_TEST_ASSERT(inSuite, establishmentReturnVal == CHIP_NO_ERROR);
        NL_TEST_ASSERT(inSuite, loopback.mSentMessageCount == testVectors[i].expectedSentMessageCount);
        NL_TEST_ASSERT(inSuite, delegateCommissioner.mNumPairingComplete == i + 1);
        SessionHolder & holder = delegateCommissioner.GetSessionHolder();
        NL_TEST_ASSERT(inSuite, bool(holder));
        NL_TEST_ASSERT(inSuite, holder->GetPeer() == fabric->GetScopedNodeIdForNode(Node01_01));
        chip::Platform::Delete(pairingCommissioner);
    }
}

// Test Suite

/**
 *  Test Suite that lists all the test functions.
 */
// clang-format off
static const nlTest sTests[] =
{
    NL_TEST_DEF("WaitInit",    CASE_SecurePairingWaitTest),
    NL_TEST_DEF("Start",       CASE_SecurePairingStartTest),
    NL_TEST_DEF("Handshake",   CASE_SecurePairingHandshakeTest),
    NL_TEST_DEF("ServerHandshake", CASE_SecurePairingHandshakeServerTest),
    NL_TEST_DEF("Sigma1Parsing", CASE_Sigma1ParsingTest),
    NL_TEST_DEF("DestinationId", CASE_DestinationIdTest),
    NL_TEST_DEF("SessionResumptionStorage", CASE_SessionResumptionStorage),

    NL_TEST_SENTINEL()
};
// clang-format on

int CASE_TestSecurePairing_Setup(void * inContext);
int CASE_TestSecurePairing_Teardown(void * inContext);

// clang-format off
static nlTestSuite sSuite =
{
    "Test-CHIP-SecurePairing-CASE",
    &sTests[0],
    CASE_TestSecurePairing_Setup,
    CASE_TestSecurePairing_Teardown,
};
// clang-format on

namespace {
/*
 *  Set up the test suite.
 */
CHIP_ERROR CASETestSecurePairingSetup(void * inContext)
{
    TestContext & ctx = *reinterpret_cast<TestContext *>(inContext);

    ctx.ConfigInitializeNodes(false);
    ReturnErrorOnFailure(ctx.Init());

    gCommissionerFabrics.Init(&gCommissionerStorageDelegate);
    gDeviceFabrics.Init(&gDeviceStorageDelegate);

    return InitCredentialSets();
}
} // anonymous namespace

/**
 *  Set up the test suite.
 */
int CASE_TestSecurePairing_Setup(void * inContext)
{
    CHIP_ERROR err = CASETestSecurePairingSetup(inContext);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Support, "Failed to init tests %" CHIP_ERROR_FORMAT, err.Format());
        return FAILURE;
    }
    return SUCCESS;
}

/**
 *  Tear down the test suite.
 */
int CASE_TestSecurePairing_Teardown(void * inContext)
{
    gCommissionerStorageDelegate.ClearStorage();
    gDeviceStorageDelegate.ClearStorage();
    gCommissionerFabrics.DeleteAllFabrics();
    gDeviceFabrics.DeleteAllFabrics();
    static_cast<TestContext *>(inContext)->Shutdown();
    return SUCCESS;
}

/**
 *  Main
 */
int TestCASESession()
{
    TestContext sContext;

    // Run test suit against one context
    nlTestRunner(&sSuite, &sContext);

    return (nlTestRunnerStats(&sSuite));
}

CHIP_REGISTER_TEST_SUITE(TestCASESession)
