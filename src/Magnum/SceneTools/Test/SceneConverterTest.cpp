/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <sstream>
#include <Corrade/Containers/ArrayTuple.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Path.h>

#include "Magnum/Math/CubicHermite.h"
#include "Magnum/Math/Matrix3.h"
#include "Magnum/Math/Matrix4.h"

#include "Magnum/SceneTools/Implementation/sceneConverterUtilities.h"

#include "configure.h"

namespace Magnum { namespace SceneTools { namespace Test { namespace {

struct SceneConverterTest: TestSuite::Tester {
    explicit SceneConverterTest();

    void infoImplementationEmpty();
    void infoImplementationScenesObjects();
    void infoImplementationAnimations();
    void infoImplementationSkins();
    void infoImplementationLights();
    void infoImplementationMaterials();
    void infoImplementationMeshes();
    void infoImplementationMeshesBounds();
    void infoImplementationTextures();
    void infoImplementationImages();
    /* Image info further tested in ImageConverterTest */
    void infoImplementationReferenceCount();
    void infoImplementationError();

    Utility::Arguments _infoArgs;
};

using namespace Math::Literals;

const struct {
    const char* name;
    const char* arg;
    const char* expected;
    bool printVisualCheck;
} InfoImplementationScenesObjectsData[]{
    {"", "--info", "info-scenes-objects.txt", true},
    {"only scenes", "--info-scenes", "info-scenes.txt", false},
    {"only objects", "--info-objects", "info-objects.txt", false},
};

const struct {
    const char* name;
    bool oneOrAll;
    bool printVisualCheck;
} InfoImplementationOneOrAllData[]{
    {"", true, true},
    {"--info", false, false},
};

SceneConverterTest::SceneConverterTest() {
    addTests({&SceneConverterTest::infoImplementationEmpty});

    addInstancedTests({&SceneConverterTest::infoImplementationScenesObjects},
        Containers::arraySize(InfoImplementationScenesObjectsData));

    addInstancedTests({&SceneConverterTest::infoImplementationAnimations,
                       &SceneConverterTest::infoImplementationSkins,
                       &SceneConverterTest::infoImplementationLights,
                       &SceneConverterTest::infoImplementationMaterials,
                       &SceneConverterTest::infoImplementationMeshes},
        Containers::arraySize(InfoImplementationOneOrAllData));

    addTests({&SceneConverterTest::infoImplementationMeshesBounds});

    addInstancedTests({&SceneConverterTest::infoImplementationTextures,
                       &SceneConverterTest::infoImplementationImages},
        Containers::arraySize(InfoImplementationOneOrAllData));

    addTests({&SceneConverterTest::infoImplementationReferenceCount,
              &SceneConverterTest::infoImplementationError});

    /* A subset of arguments needed by the info printing code */
    _infoArgs.addBooleanOption("info")
             .addBooleanOption("info-scenes")
             .addBooleanOption("info-objects")
             .addBooleanOption("info-animations")
             .addBooleanOption("info-skins")
             .addBooleanOption("info-lights")
             .addBooleanOption("info-materials")
             .addBooleanOption("info-meshes")
             .addBooleanOption("info-textures")
             .addBooleanOption("info-images")
             .addBooleanOption("bounds");
}

void SceneConverterTest::infoImplementationEmpty() {
    struct Importer: Trade::AbstractImporter {
        Trade::ImporterFeatures doFeatures() const override { return {}; }
        bool doIsOpened() const override { return true; }
        void doClose() override {}
    } importer;

    const char* argv[]{"", "--info"};
    CORRADE_VERIFY(_infoArgs.tryParse(Containers::arraySize(argv), argv));

    std::chrono::high_resolution_clock::duration time;

    std::ostringstream out;
    Debug redirectOutput{&out};
    CORRADE_VERIFY(Implementation::printInfo(Debug::Flag::DisableColors, {}, _infoArgs, importer, time) == false);
    CORRADE_COMPARE(out.str(), "");
}

void SceneConverterTest::infoImplementationScenesObjects() {
    auto&& data = InfoImplementationScenesObjectsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    struct Importer: Trade::AbstractImporter {
        Trade::ImporterFeatures doFeatures() const override { return {}; }
        bool doIsOpened() const override { return true; }
        void doClose() override {}

        /* First scene has 4, second 7, the last three are not in any scene and
           thus not listed. Object 5 has no fields and thus not listed either. */
        UnsignedLong doObjectCount() const override { return 10; }
        UnsignedInt doSceneCount() const override { return 2; }
        Containers::String doSceneName(UnsignedInt id) override {
            return id == 0 ? "A simple scene" : "";
        }
        Containers::String doObjectName(UnsignedLong id) override {
            if(id == 0) return "Parent-less mesh";
            if(id == 2) return "Two meshes, shared among two scenes";
            if(id == 4) return "Two custom arrays";
            if(id == 6) return "Only in the second scene, but no fields, thus same as unreferenced";
            if(id == 8) return "Not in any scene";
            return "";
        }
        Containers::String doSceneFieldName(UnsignedInt name) override {
            if(name == 1337) return "DirectionVector";
            return "";
        }
        Containers::Optional<Trade::SceneData> doScene(UnsignedInt id) override {
            /* Builtin fields, some duplicated, one marked as ordered */
            if(id == 0) {
                Containers::ArrayView<UnsignedInt> parentMapping;
                Containers::ArrayView<Int> parents;
                Containers::ArrayView<UnsignedInt> meshMapping;
                Containers::ArrayView<UnsignedInt> meshes;
                Containers::ArrayTuple data{
                    {NoInit, 3, parentMapping},
                    {ValueInit, 3, parents},
                    {NoInit, 4, meshMapping},
                    {ValueInit, 4, meshes},
                };
                Utility::copy({1, 3, 2}, parentMapping);
                Utility::copy({2, 0, 2, 1}, meshMapping);
                /* No need to fill the data, zero-init is fine */
                return Trade::SceneData{Trade::SceneMappingType::UnsignedInt, 4, std::move(data), {
                    Trade::SceneFieldData{Trade::SceneField::Parent, parentMapping, parents},
                    Trade::SceneFieldData{Trade::SceneField::Mesh, meshMapping, meshes, Trade::SceneFieldFlag::OrderedMapping},
                }};
            }

            /* Two custom fields, one array. Stored as an external memory. */
            if(id == 1) {
                return Trade::SceneData{Trade::SceneMappingType::UnsignedByte, 8, Trade::DataFlag::ExternallyOwned|Trade::DataFlag::Mutable, scene2Data, {
                    Trade::SceneFieldData{Trade::sceneFieldCustom(42), Containers::arrayView(scene2Data->customMapping), Containers::arrayView(scene2Data->custom)},
                    Trade::SceneFieldData{Trade::sceneFieldCustom(1337), Trade::SceneMappingType::UnsignedByte, scene2Data->customArrayMapping, Trade::SceneFieldType::Short, scene2Data->customArray, 3},
                }};
            }

            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        struct {
            UnsignedByte customMapping[2];
            Double custom[2];
            UnsignedByte customArrayMapping[3];
            Vector3s customArray[3];
        } scene2Data[1]{{
            /* No need to fill the data, zero-init is fine */
            {7, 3}, {}, {2, 4, 4}, {}
        }};
    } importer;

    const char* argv[]{"", data.arg};
    CORRADE_VERIFY(_infoArgs.tryParse(Containers::arraySize(argv), argv));

    std::chrono::high_resolution_clock::duration time;

    /* Print to visually verify coloring */
    if(data.printVisualCheck) {
        Debug{} << "======================== visual color verification start =======================";
        Implementation::printInfo(Debug::isTty() ? Debug::Flags{} : Debug::Flag::DisableColors, Debug::isTty(), _infoArgs, importer, time);
        Debug{} << "======================== visual color verification end =========================";
    }

    std::ostringstream out;
    Debug redirectOutput{&out};
    CORRADE_VERIFY(Implementation::printInfo(Debug::Flag::DisableColors, {}, _infoArgs, importer, time) == false);
    CORRADE_COMPARE_AS(out.str(),
        Utility::Path::join({SCENETOOLS_TEST_DIR, "SceneConverterTestFiles", data.expected}),
        TestSuite::Compare::StringToFile);
}

void SceneConverterTest::infoImplementationAnimations() {
    auto&& data = InfoImplementationOneOrAllData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    struct Importer: Trade::AbstractImporter {
        Trade::ImporterFeatures doFeatures() const override { return {}; }
        bool doIsOpened() const override { return true; }
        void doClose() override {}

        UnsignedInt doAnimationCount() const override { return 2; }
        Containers::String doAnimationName(UnsignedInt id) override {
            return id == 1 ? "Custom track duration and interpolator function" : "";
        }
        Containers::Optional<Trade::AnimationData> doAnimation(UnsignedInt id) override {
            /* First has two tracks with a shared time and implicit duration,
               one with a different result type. */
            if(id == 0) {
                Containers::ArrayView<Float> time;
                Containers::ArrayView<Vector2> translation;
                Containers::ArrayView<CubicHermite2D> rotation;
                Containers::ArrayTuple data{
                    {ValueInit, 3, time},
                    {ValueInit, 3, translation},
                    {ValueInit, 3, rotation}
                };
                Utility::copy({0.5f, 1.0f, 1.25f}, time);
                return Trade::AnimationData{std::move(data), {
                    /** @todo cleanup once AnimationTrackData has sane
                        constructors */
                    Trade::AnimationTrackData{Trade::AnimationTrackTargetType::Translation2D, 17, Animation::TrackView<const Float, const Vector2>{time, translation, Animation::Interpolation::Linear, Animation::Extrapolation::DefaultConstructed, Animation::Extrapolation::Constant}},
                    Trade::AnimationTrackData{Trade::AnimationTrackTargetType::Rotation2D, 17, Animation::TrackView<const Float, const CubicHermite2D>{time, rotation, Animation::Interpolation::Constant, Animation::Extrapolation::Extrapolated}},
                }};
            }

            /* Second has track duration different from animation duration and
               a custom interpolator. Stored as an external memory. */
            if(id == 1) {
                return Trade::AnimationData{Trade::DataFlag::ExternallyOwned, animation2Data, {
                    /** @todo cleanup once AnimationTrackData has sane
                        constructors */
                    Trade::AnimationTrackData{Trade::AnimationTrackTargetType::Scaling3D, 666, Animation::TrackView<const Float, const Vector3>{animation2Data->time, animation2Data->scaling, Math::lerp, Animation::Extrapolation::DefaultConstructed, Animation::Extrapolation::Constant}},
                }, {0.1f, 1.3f}};
            }

            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        struct {
            Float time[5];
            Vector3 scaling[5];
        } animation2Data[1]{{
            {0.75f, 0.75f, 1.0f, 1.0f, 1.25f},
            {}
        }};
    } importer;

    const char* argv[]{"", data.oneOrAll ? "--info-animations" : "--info"};
    CORRADE_VERIFY(_infoArgs.tryParse(Containers::arraySize(argv), argv));

    std::chrono::high_resolution_clock::duration time;

    /* Print to visually verify coloring */
    if(data.printVisualCheck) {
        Debug{} << "======================== visual color verification start =======================";
        Implementation::printInfo(Debug::isTty() ? Debug::Flags{} : Debug::Flag::DisableColors, Debug::isTty(), _infoArgs, importer, time);
        Debug{} << "======================== visual color verification end =========================";
    }

    std::ostringstream out;
    Debug redirectOutput{&out};
    CORRADE_VERIFY(Implementation::printInfo(Debug::Flag::DisableColors, {}, _infoArgs, importer, time) == false);
    CORRADE_COMPARE_AS(out.str(),
        Utility::Path::join(SCENETOOLS_TEST_DIR, "SceneConverterTestFiles/info-animations.txt"),
        TestSuite::Compare::StringToFile);
}

void SceneConverterTest::infoImplementationSkins() {
    auto&& data = InfoImplementationOneOrAllData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    struct Importer: Trade::AbstractImporter {
        Trade::ImporterFeatures doFeatures() const override { return {}; }
        bool doIsOpened() const override { return true; }
        void doClose() override {}

        UnsignedInt doSkin2DCount() const override { return 2; }
        Containers::String doSkin2DName(UnsignedInt id) override {
            return id == 1 ? "Second 2D skin, external data" : "";
        }
        Containers::Optional<Trade::SkinData2D> doSkin2D(UnsignedInt id) override {
            /* First a regular skin, second externally owned */
            if(id == 0) return Trade::SkinData2D{
                {3, 6, 7, 12, 22},
                {{}, {}, {}, {}, {}}
            };

            if(id == 1) return Trade::SkinData2D{Trade::DataFlag::ExternallyOwned, skin2JointData, Trade::DataFlag::ExternallyOwned, skin2MatrixData};

            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        UnsignedInt doSkin3DCount() const override { return 3; }
        Containers::String doSkin3DName(UnsignedInt id) override {
            return id == 0 ? "First 3D skin, external data" : "";
        }
        Containers::Optional<Trade::SkinData3D> doSkin3D(UnsignedInt id) override {
            /* Reverse order in 3D, plus one more to ensure the count isn't
               mismatched between 2D and 3D */
            if(id == 0) return Trade::SkinData3D{Trade::DataFlag::ExternallyOwned, skin3JointData, Trade::DataFlag::ExternallyOwned, skin3MatrixData};

            if(id == 1) return Trade::SkinData3D{
                {3, 22},
                {{}, {}}
            };

            if(id == 2) return Trade::SkinData3D{
                {3},
                {{}}
            };

            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        UnsignedInt skin2JointData[15];
        Matrix3 skin2MatrixData[15];
        UnsignedInt skin3JointData[12];
        Matrix4 skin3MatrixData[12];
    } importer;

    const char* argv[]{"", data.oneOrAll ? "--info-skins" : "--info"};
    CORRADE_VERIFY(_infoArgs.tryParse(Containers::arraySize(argv), argv));

    std::chrono::high_resolution_clock::duration time;

    /* Print to visually verify coloring */
    if(data.printVisualCheck) {
        Debug{} << "======================== visual color verification start =======================";
        Implementation::printInfo(Debug::isTty() ? Debug::Flags{} : Debug::Flag::DisableColors, Debug::isTty(), _infoArgs, importer, time);
        Debug{} << "======================== visual color verification end =========================";
    }

    std::ostringstream out;
    Debug redirectOutput{&out};
    CORRADE_VERIFY(Implementation::printInfo(Debug::Flag::DisableColors, {}, _infoArgs, importer, time) == false);
    CORRADE_COMPARE_AS(out.str(),
        Utility::Path::join(SCENETOOLS_TEST_DIR, "SceneConverterTestFiles/info-skins.txt"),
        TestSuite::Compare::StringToFile);
}

void SceneConverterTest::infoImplementationLights() {
    auto&& data = InfoImplementationOneOrAllData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    struct Importer: Trade::AbstractImporter {
        Trade::ImporterFeatures doFeatures() const override { return {}; }
        bool doIsOpened() const override { return true; }
        void doClose() override {}

        UnsignedInt doLightCount() const override { return 2; }
        Containers::String doLightName(UnsignedInt id) override {
            return id == 1 ?  "Directional light with always-implicit attenuation and range" : "";
        }
        Containers::Optional<Trade::LightData> doLight(UnsignedInt id) override {
            /* First a blue spot light */
            if(id == 0) return Trade::LightData{
                Trade::LightData::Type::Spot,
                0x3457ff_rgbf,
                15.0f,
                {1.2f, 0.3f, 0.04f},
                100.0f,
                55.0_degf,
                85.0_degf
            };

            /* Second a yellow directional light with infinite range */
            if(id == 1) return Trade::LightData{
                Trade::LightData::Type::Directional,
                0xff5734_rgbf,
                5.0f
            };

            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }
    } importer;

    const char* argv[]{"", data.oneOrAll ? "--info-lights" : "--info"};
    CORRADE_VERIFY(_infoArgs.tryParse(Containers::arraySize(argv), argv));

    std::chrono::high_resolution_clock::duration time;

    /* Print to visually verify coloring */
    if(data.printVisualCheck) {
        Debug{} << "======================== visual color verification start =======================";
        Implementation::printInfo(Debug::isTty() ? Debug::Flags{} : Debug::Flag::DisableColors, Debug::isTty(), _infoArgs, importer, time);
        Debug{} << "======================== visual color verification end =========================";
    }

    std::ostringstream out;
    Debug redirectOutput{&out};
    CORRADE_VERIFY(Implementation::printInfo(Debug::Flag::DisableColors, false, _infoArgs, importer, time) == false);
    CORRADE_COMPARE_AS(out.str(),
        Utility::Path::join(SCENETOOLS_TEST_DIR, "SceneConverterTestFiles/info-lights.txt"),
        TestSuite::Compare::StringToFile);
}

void SceneConverterTest::infoImplementationMaterials() {
    auto&& data = InfoImplementationOneOrAllData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    struct Importer: Trade::AbstractImporter {
        Trade::ImporterFeatures doFeatures() const override { return {}; }
        bool doIsOpened() const override { return true; }
        void doClose() override {}

        UnsignedInt doMaterialCount() const override { return 2; }
        Containers::String doMaterialName(UnsignedInt id) override {
            return id == 1 ? "Lots o' laierz" : "";
        }
        Containers::Optional<Trade::MaterialData> doMaterial(UnsignedInt id) override {
            /* First has custom attributes */
            if(id == 0) return Trade::MaterialData{Trade::MaterialType::PbrMetallicRoughness, {
                {Trade::MaterialAttribute::BaseColor, 0x3bd26799_rgbaf},
                {Trade::MaterialAttribute::DoubleSided, true},
                {Trade::MaterialAttribute::EmissiveColor, 0xe9eca_rgbf},
                {Trade::MaterialAttribute::RoughnessTexture, 67u},
                {Trade::MaterialAttribute::RoughnessTextureMatrix, Matrix3::translation({0.25f, 0.75f})},
                {Trade::MaterialAttribute::RoughnessTextureSwizzle, Trade::MaterialTextureSwizzle::B},
                {"reflectionAngle", 35.0_degf},
                /* These shouldn't have a color swatch rendered */
                {"notAColour4", Vector4{0.1f, 0.2f, 0.3f, 0.4f}},
                {"notAColour3", Vector3{0.2f, 0.3f, 0.4f}},
                {"deadBeef", reinterpret_cast<const void*>(0xdeadbeef)},
                {"undeadBeef", reinterpret_cast<void*>(0xbeefbeef)},
            }};

            /* Second has layers, custom layers, unnamed layers and a name */
            if(id == 1) return Trade::MaterialData{Trade::MaterialType::PbrClearCoat|Trade::MaterialType::Phong, {
                {Trade::MaterialAttribute::DiffuseColor, 0xc7cf2f99_rgbaf},
                {Trade::MaterialLayer::ClearCoat},
                {Trade::MaterialAttribute::LayerFactor, 0.5f},
                {Trade::MaterialAttribute::LayerFactorTexture, 3u},
                {Trade::MaterialAttribute::LayerName, "anEmptyLayer"},
                {Trade::MaterialAttribute::LayerFactor, 0.25f},
                {Trade::MaterialAttribute::LayerFactorTexture, 2u},
                {"yes", "a string"},
            }, {1, 4, 5, 8}};

            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }
    } importer;

    const char* argv[]{"", data.oneOrAll ? "--info-materials" : "--info"};
    CORRADE_VERIFY(_infoArgs.tryParse(Containers::arraySize(argv), argv));

    std::chrono::high_resolution_clock::duration time;

    /* Print to visually verify coloring */
    if(data.printVisualCheck) {
        Debug{} << "======================== visual color verification start =======================";
        Implementation::printInfo(Debug::isTty() ? Debug::Flags{} : Debug::Flag::DisableColors, Debug::isTty(), _infoArgs, importer, time);
        Debug{} << "======================== visual color verification end =========================";
    }

    std::ostringstream out;
    Debug redirectOutput{&out};
    CORRADE_VERIFY(Implementation::printInfo(Debug::Flag::DisableColors, false, _infoArgs, importer, time) == false);
    CORRADE_COMPARE_AS(out.str(),
        Utility::Path::join(SCENETOOLS_TEST_DIR, "SceneConverterTestFiles/info-materials.txt"),
        TestSuite::Compare::StringToFile);
}

void SceneConverterTest::infoImplementationMeshes() {
    auto&& data = InfoImplementationOneOrAllData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    struct Importer: Trade::AbstractImporter {
        Trade::ImporterFeatures doFeatures() const override { return {}; }
        bool doIsOpened() const override { return true; }
        void doClose() override {}

        UnsignedInt doMeshCount() const override { return 3; }
        UnsignedInt doMeshLevelCount(UnsignedInt id) override {
            return id == 1 ? 2 : 1;
        }
        Containers::String doMeshName(UnsignedInt id) override {
            return id == 1 ? "LODs? No, meshets." : "";
        }
        Containers::String doMeshAttributeName(UnsignedShort name) override {
            if(name == 25) return "vertices";
            if(name == 26) return "triangles";
            /* 37 (triangleCount) deliberately not named */
            if(name == 116) return "vertexCount";

            return "";
        }
        Containers::Optional<Trade::MeshData> doMesh(UnsignedInt id, UnsignedInt level) override {
            /* First is indexed & externally owned */
            if(id == 0 && level == 0) return Trade::MeshData{MeshPrimitive::Points,
                Trade::DataFlag::ExternallyOwned, indices,
                Trade::MeshIndexData{indices},
                Trade::DataFlag::ExternallyOwned|Trade::DataFlag::Mutable, points, {
                    Trade::MeshAttributeData{Trade::MeshAttribute::Position, Containers::arrayView(points)}
                }};

            /* Second is multi-level, with second level being indexed meshlets
               with custom (array) attributes */
            if(id == 1 && level == 0) {
                Containers::ArrayView<Vector3> positions;
                Containers::ArrayView<Vector4> tangents;
                Containers::ArrayTuple data{
                    {NoInit, 250, positions},
                    {NoInit, 250, tangents},
                };
                return Trade::MeshData{MeshPrimitive::Triangles, std::move(data), {
                    Trade::MeshAttributeData{Trade::MeshAttribute::Position, positions},
                    Trade::MeshAttributeData{Trade::MeshAttribute::Tangent, tangents},
                }};
            }
            if(id == 1 && level == 1) {
                Containers::StridedArrayView2D<UnsignedInt> vertices;
                Containers::StridedArrayView2D<Vector3ub> indices;
                Containers::ArrayView<UnsignedByte> triangleCount;
                Containers::ArrayView<UnsignedByte> vertexCount;
                Containers::ArrayTuple data{
                    {NoInit, {135, 64}, vertices},
                    {NoInit, {135, 126}, indices},
                    {NoInit, 135, triangleCount},
                    {NoInit, 135, vertexCount},
                };
                return Trade::MeshData{MeshPrimitive::Meshlets, std::move(data), {
                    Trade::MeshAttributeData{Trade::meshAttributeCustom(25), vertices},
                    Trade::MeshAttributeData{Trade::meshAttributeCustom(26), indices},
                    Trade::MeshAttributeData{Trade::meshAttributeCustom(37), triangleCount},
                    Trade::MeshAttributeData{Trade::meshAttributeCustom(116), vertexCount},
                }};
            }

            /* Third is an empty instance mesh */
            if(id == 2 && level == 0) return Trade::MeshData{MeshPrimitive::Instances, 15};

            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        UnsignedShort indices[70];
        Vector3 points[50];
    } importer;

    const char* argv[]{"", data.oneOrAll ? "--info-meshes" : "--info"};
    CORRADE_VERIFY(_infoArgs.tryParse(Containers::arraySize(argv), argv));

    std::chrono::high_resolution_clock::duration time;

    /* Print to visually verify coloring */
    if(data.printVisualCheck) {
        Debug{} << "======================== visual color verification start =======================";
        Implementation::printInfo(Debug::isTty() ? Debug::Flags{} : Debug::Flag::DisableColors, Debug::isTty(), _infoArgs, importer, time);
        Debug{} << "======================== visual color verification end =========================";
    }

    std::ostringstream out;
    Debug redirectOutput{&out};
    CORRADE_VERIFY(Implementation::printInfo(Debug::Flag::DisableColors, false, _infoArgs, importer, time) == false);
    CORRADE_COMPARE_AS(out.str(),
        Utility::Path::join(SCENETOOLS_TEST_DIR, "SceneConverterTestFiles/info-meshes.txt"),
        TestSuite::Compare::StringToFile);
}

void SceneConverterTest::infoImplementationMeshesBounds() {
    struct Importer: Trade::AbstractImporter {
        Trade::ImporterFeatures doFeatures() const override { return {}; }
        bool doIsOpened() const override { return true; }
        void doClose() override {}

        UnsignedInt doMeshCount() const override { return 1; }
        Containers::Optional<Trade::MeshData> doMesh(UnsignedInt, UnsignedInt) override {
            return Trade::MeshData{MeshPrimitive::Lines,
                {}, indexData, Trade::MeshIndexData{indexData},
                {}, vertexData, {
                    Trade::MeshAttributeData{Trade::MeshAttribute::Position, Containers::arrayView(vertexData->positions)},
                    Trade::MeshAttributeData{Trade::MeshAttribute::Tangent, Containers::arrayView(vertexData->tangent)},
                    Trade::MeshAttributeData{Trade::MeshAttribute::Bitangent, Containers::arrayView(vertexData->bitangent)},
                    Trade::MeshAttributeData{Trade::MeshAttribute::ObjectId, Containers::arrayView(vertexData->objectId)},
                    Trade::MeshAttributeData{Trade::MeshAttribute::Normal, Containers::arrayView(vertexData->normal)},
                    Trade::MeshAttributeData{Trade::MeshAttribute::TextureCoordinates, Containers::arrayView(vertexData->textureCoordinates)},
                    Trade::MeshAttributeData{Trade::MeshAttribute::Color, Containers::arrayView(vertexData->color)},
                    Trade::MeshAttributeData{Trade::MeshAttribute::ObjectId, Containers::arrayView(vertexData->objectIdSecondary)},
                }};
        }

        UnsignedByte indexData[3]{15, 3, 176};

        struct {
            Vector3 positions[2];
            Vector3 tangent[2];
            Vector3 bitangent[2];
            UnsignedShort objectId[2];
            Vector3 normal[2];
            Vector2 textureCoordinates[2];
            Vector4 color[2];
            UnsignedInt objectIdSecondary[2];
        } vertexData[1]{{
            {{0.1f, -0.1f, 0.2f}, {0.2f, 0.0f, -0.2f}},
            {{0.2f, -0.2f, 0.8f}, {0.3f, 0.8f, 0.2f}},
            {{0.4f, 0.2f, 1.0f}, {0.3f, 0.9f, 0.0f}},
            {155, 12},
            {{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f}, {1.5f, 0.5f}},
            {0x99336600_rgbaf, 0xff663333_rgbaf},
            {15, 337},
        }};
    } importer;

    const char* argv[]{"", "--info-meshes", "--bounds"};
    CORRADE_VERIFY(_infoArgs.tryParse(Containers::arraySize(argv), argv));

    std::chrono::high_resolution_clock::duration time;

    /* Print to visually verify coloring */
    {
        Debug{} << "======================== visual color verification start =======================";
        Implementation::printInfo(Debug::isTty() ? Debug::Flags{} : Debug::Flag::DisableColors, Debug::isTty(), _infoArgs, importer, time);
        Debug{} << "======================== visual color verification end =========================";
    }

    std::ostringstream out;
    Debug redirectOutput{&out};
    CORRADE_VERIFY(Implementation::printInfo(Debug::Flag::DisableColors, false, _infoArgs, importer, time) == false);
    CORRADE_COMPARE_AS(out.str(),
        Utility::Path::join(SCENETOOLS_TEST_DIR, "SceneConverterTestFiles/info-meshes-bounds.txt"),
        TestSuite::Compare::StringToFile);
}

void SceneConverterTest::infoImplementationTextures() {
    auto&& data = InfoImplementationOneOrAllData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    struct Importer: Trade::AbstractImporter {
        Trade::ImporterFeatures doFeatures() const override { return {}; }
        bool doIsOpened() const override { return true; }
        void doClose() override {}

        UnsignedInt doTextureCount() const override { return 2; }
        Containers::String doTextureName(UnsignedInt id) override {
            return id == 1 ? "Name!" : "";
        }
        Containers::Optional<Trade::TextureData> doTexture(UnsignedInt id) override {
            /* First a 1D texture */
            if(id == 0) return Trade::TextureData{
                Trade::TextureType::Texture1D,
                SamplerFilter::Nearest,
                SamplerFilter::Linear,
                SamplerMipmap::Nearest,
                SamplerWrapping::Repeat,
                666
            };

            /* Second a 2D array texture */
            if(id == 1) return Trade::TextureData{
                Trade::TextureType::Texture2DArray,
                SamplerFilter::Linear,
                SamplerFilter::Nearest,
                SamplerMipmap::Linear,
                {SamplerWrapping::MirroredRepeat, SamplerWrapping::ClampToEdge, SamplerWrapping::MirrorClampToEdge},
                3
            };

            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }
    } importer;

    const char* argv[]{"", data.oneOrAll ? "--info-textures" : "--info"};
    CORRADE_VERIFY(_infoArgs.tryParse(Containers::arraySize(argv), argv));

    std::chrono::high_resolution_clock::duration time;

    /* Print to visually verify coloring */
    if(data.printVisualCheck) {
        Debug{} << "======================== visual color verification start =======================";
        Implementation::printInfo(Debug::isTty() ? Debug::Flags{} : Debug::Flag::DisableColors, Debug::isTty(), _infoArgs, importer, time);
        Debug{} << "======================== visual color verification end =========================";
    }

    std::ostringstream out;
    Debug redirectOutput{&out};
    CORRADE_VERIFY(Implementation::printInfo(Debug::Flag::DisableColors, false, _infoArgs, importer, time) == false);
    CORRADE_COMPARE_AS(out.str(),
        Utility::Path::join(SCENETOOLS_TEST_DIR, "SceneConverterTestFiles/info-textures.txt"),
        TestSuite::Compare::StringToFile);
}

void SceneConverterTest::infoImplementationImages() {
    auto&& data = InfoImplementationOneOrAllData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* Just the very basics to ensure image info *is* printed. Tested in full
       in ImageConverterTest. */
    struct Importer: Trade::AbstractImporter {
        Trade::ImporterFeatures doFeatures() const override { return {}; }
        bool doIsOpened() const override { return true; }
        void doClose() override {}

        UnsignedInt doImage1DCount() const override { return 1; }
        Containers::Optional<Trade::ImageData1D> doImage1D(UnsignedInt, UnsignedInt) override {
            return Trade::ImageData1D{PixelFormat::R32F, 1024, Containers::Array<char>{NoInit, 4096}};
        }
    } importer;

    const char* argv[]{"", data.oneOrAll ? "--info-images" : "--info"};
    CORRADE_VERIFY(_infoArgs.tryParse(Containers::arraySize(argv), argv));

    std::chrono::high_resolution_clock::duration time;

    /* Print to visually verify coloring */
    if(data.printVisualCheck) {
        Debug{} << "======================== visual color verification start =======================";
        Implementation::printInfo(Debug::isTty() ? Debug::Flags{} : Debug::Flag::DisableColors, Debug::isTty(), _infoArgs, importer, time);
        Debug{} << "======================== visual color verification end =========================";
    }

    std::ostringstream out;
    Debug redirectOutput{&out};
    CORRADE_VERIFY(Implementation::printInfo(Debug::Flag::DisableColors, false, _infoArgs, importer, time) == false);
    CORRADE_COMPARE_AS(out.str(),
        Utility::Path::join(SCENETOOLS_TEST_DIR, "SceneConverterTestFiles/info-images.txt"),
        TestSuite::Compare::StringToFile);
}

void SceneConverterTest::infoImplementationReferenceCount() {
    struct Importer: Trade::AbstractImporter {
        Trade::ImporterFeatures doFeatures() const override { return {}; }
        bool doIsOpened() const override { return true; }
        void doClose() override {}

        /* One data of each kind should be always referenced twice+, one once,
           one not at all, and one reference should be OOB */

        UnsignedLong doObjectCount() const override { return 4; }
        Containers::String doObjectName(UnsignedLong id) override {
            return id == 2 ? "Not referenced" : "";
        }
        UnsignedInt doSceneCount() const override { return 2; }
        Containers::Optional<Trade::SceneData> doScene(UnsignedInt id) override {
            if(id == 0) return Trade::SceneData{Trade::SceneMappingType::UnsignedInt, 2, {}, sceneData3D, {
                /* To mark the scene as 3D */
                Trade::SceneFieldData{Trade::SceneField::Transformation, Trade::SceneMappingType::UnsignedInt, nullptr, Trade::SceneFieldType::Matrix4x4, nullptr},
                Trade::SceneFieldData{Trade::SceneField::Mesh,
                    Containers::arrayView(sceneData3D->mapping),
                    Containers::arrayView(sceneData3D->meshes)},
                Trade::SceneFieldData{Trade::SceneField::MeshMaterial,
                    Containers::arrayView(sceneData3D->mapping),
                    Containers::arrayView(sceneData3D->materials)},
                Trade::SceneFieldData{Trade::SceneField::Light,
                    Containers::arrayView(sceneData3D->mapping),
                    Containers::arrayView(sceneData3D->lights)},
                Trade::SceneFieldData{Trade::SceneField::Skin,
                    Containers::arrayView(sceneData3D->mapping),
                    Containers::arrayView(sceneData3D->skins)},
            }};
            if(id == 1) return Trade::SceneData{Trade::SceneMappingType::UnsignedInt, 4, {}, sceneData2D, {
                /* To mark the scene as 2D */
                Trade::SceneFieldData{Trade::SceneField::Transformation, Trade::SceneMappingType::UnsignedInt, nullptr, Trade::SceneFieldType::Matrix3x3, nullptr},
                Trade::SceneFieldData{Trade::SceneField::Mesh,
                    Containers::arrayView(sceneData2D->mapping),
                    Containers::arrayView(sceneData2D->meshes)},
                Trade::SceneFieldData{Trade::SceneField::Skin,
                    Containers::arrayView(sceneData2D->mapping),
                    Containers::arrayView(sceneData2D->skins)},
            }};

            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        UnsignedInt doSkin2DCount() const override { return 3; }
        Containers::String doSkin2DName(UnsignedInt id) override {
            return id == 2 ? "Not referenced" : "";
        }
        Containers::Optional<Trade::SkinData2D> doSkin2D(UnsignedInt id) override {
            if(id == 0) return Trade::SkinData2D{
                {35, 22},
                {{}, {}}
            };
            if(id == 1) return Trade::SkinData2D{
                {33, 10, 100},
                {{}, {}, {}}
            };
            if(id == 2) return Trade::SkinData2D{
                {66},
                {{}}
            };

            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        UnsignedInt doSkin3DCount() const override { return 3; }
        Containers::String doSkin3DName(UnsignedInt id) override {
            return id == 0 ? "Not referenced" : "";
        }
        Containers::Optional<Trade::SkinData3D> doSkin3D(UnsignedInt id) override {
            if(id == 0) return Trade::SkinData3D{
                {35, 22},
                {{}, {}}
            };
            if(id == 1) return Trade::SkinData3D{
                {37},
                {{}}
            };
            if(id == 2) return Trade::SkinData3D{
                {300, 10, 1000},
                {{}, {}, {}}
            };

            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        UnsignedInt doLightCount() const override { return 3; }
        Containers::String doLightName(UnsignedInt id) override {
            return id == 1 ? "Not referenced" : "";
        }
        Containers::Optional<Trade::LightData> doLight(UnsignedInt id) override {
            if(id == 0) return Trade::LightData{
                Trade::LightData::Type::Directional,
                0x57ff34_rgbf,
                5.0f
            };
            if(id == 1) return Trade::LightData{
                Trade::LightData::Type::Ambient,
                0xff5734_rgbf,
                0.1f
            };
            if(id == 2) return Trade::LightData{
                Trade::LightData::Type::Directional,
                0x3457ff_rgbf,
                1.0f
            };
            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        UnsignedInt doMaterialCount() const override { return 3; }
        Containers::String doMaterialName(UnsignedInt id) override {
            return id == 2 ? "Not referenced" : "";
        }
        Containers::Optional<Trade::MaterialData> doMaterial(UnsignedInt id) override {
            if(id == 0) return Trade::MaterialData{{}, {
                {Trade::MaterialAttribute::DiffuseTexture, 2u},
                {Trade::MaterialAttribute::BaseColorTexture, 2u},
            }};
            if(id == 1) return Trade::MaterialData{{}, {
                {"lookupTexture", 0u},
                {"volumeTexture", 3u},
                {Trade::MaterialAttribute::NormalTexture, 17u},
                {Trade::MaterialAttribute::EmissiveTexture, 4u},
            }};
            if(id == 2) return Trade::MaterialData{{}, {}};
            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        UnsignedInt doMeshCount() const override { return 3; }
        Containers::String doMeshName(UnsignedInt id) override {
            return id == 1 ? "Not referenced" : "";
        }
        Containers::Optional<Trade::MeshData> doMesh(UnsignedInt id, UnsignedInt) override {
            if(id == 0) return Trade::MeshData{MeshPrimitive::Points, 5};
            if(id == 1) return Trade::MeshData{MeshPrimitive::Lines, 4};
            if(id == 2) return Trade::MeshData{MeshPrimitive::TriangleFan, 4};
            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        UnsignedInt doTextureCount() const override { return 5; }
        Containers::String doTextureName(UnsignedInt id) override {
            return id == 1 ? "Not referenced" : "";
        }
        Containers::Optional<Trade::TextureData> doTexture(UnsignedInt id) override {
            if(id == 0) return Trade::TextureData{
                Trade::TextureType::Texture1D,
                SamplerFilter::Nearest,
                SamplerFilter::Linear,
                SamplerMipmap::Nearest,
                SamplerWrapping::Repeat,
                1
            };
            if(id == 1) return Trade::TextureData{
                Trade::TextureType::Texture1DArray,
                SamplerFilter::Nearest,
                SamplerFilter::Linear,
                SamplerMipmap::Nearest,
                SamplerWrapping::Repeat,
                225
            };
            if(id == 2) return Trade::TextureData{
                Trade::TextureType::Texture2D,
                SamplerFilter::Nearest,
                SamplerFilter::Linear,
                SamplerMipmap::Nearest,
                SamplerWrapping::Repeat,
                0
            };
            if(id == 3) return Trade::TextureData{
                Trade::TextureType::Texture3D,
                SamplerFilter::Nearest,
                SamplerFilter::Linear,
                SamplerMipmap::Nearest,
                SamplerWrapping::Repeat,
                1
            };
            if(id == 4) return Trade::TextureData{
                Trade::TextureType::Texture2D,
                SamplerFilter::Nearest,
                SamplerFilter::Linear,
                SamplerMipmap::Nearest,
                SamplerWrapping::Repeat,
                0
            };
            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        UnsignedInt doImage1DCount() const override { return 2; }
        Containers::String doImage1DName(UnsignedInt id) override {
            return id == 0 ? "Not referenced" : "";
        }
        Containers::Optional<Trade::ImageData1D> doImage1D(UnsignedInt id, UnsignedInt) override {
            if(id == 0)
                return Trade::ImageData1D{PixelFormat::RGBA8I, 1, Containers::Array<char>{NoInit, 4}};
            if(id == 1)
                return Trade::ImageData1D{PixelFormat::R8I, 4, Containers::Array<char>{NoInit, 4}};
            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        UnsignedInt doImage2DCount() const override { return 2; }
        Containers::String doImage2DName(UnsignedInt id) override {
            return id == 1 ? "Not referenced" : "";
        }
        Containers::Optional<Trade::ImageData2D> doImage2D(UnsignedInt id, UnsignedInt) override {
            if(id == 0)
                return Trade::ImageData2D{PixelFormat::RGBA8I, {1, 2}, Containers::Array<char>{NoInit, 8}};
            if(id == 1)
                return Trade::ImageData2D{PixelFormat::R8I, {4, 1}, Containers::Array<char>{NoInit, 4}};
            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        UnsignedInt doImage3DCount() const override { return 2; }
        Containers::String doImage3DName(UnsignedInt id) override {
            return id == 0 ? "Not referenced" : "";
        }
        Containers::Optional<Trade::ImageData3D> doImage3D(UnsignedInt id, UnsignedInt) override {
            if(id == 0)
                return Trade::ImageData3D{PixelFormat::RGBA8I, {1, 2, 1}, Containers::Array<char>{NoInit, 8}};
            if(id == 1)
                return Trade::ImageData3D{PixelFormat::R8I, {4, 1, 1}, Containers::Array<char>{NoInit, 4}};
            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        struct {
            UnsignedInt mapping[4];
            UnsignedInt meshes[4];
            Int materials[4];
            UnsignedInt lights[4];
            UnsignedInt skins[4];
        } sceneData3D[1]{{
            {0, 1, 1, 25},
            {2, 0, 2, 67},
            {0, 1, 23, 0},
            {0, 17, 0, 2},
            {1, 1, 22, 2}
        }};

        struct {
            UnsignedInt mapping[3];
            UnsignedInt meshes[3];
            UnsignedInt skins[3];
        } sceneData2D[1]{{
            {3, 116, 1},
            {2, 0, 23},
            {177, 0, 1}
        }};
    } importer;

    const char* argv[]{"", "--info"};
    CORRADE_VERIFY(_infoArgs.tryParse(Containers::arraySize(argv), argv));

    std::chrono::high_resolution_clock::duration time;

    /* Print to visually verify coloring */
    {
        Debug{} << "======================== visual color verification start =======================";
        Implementation::printInfo(Debug::isTty() ? Debug::Flags{} : Debug::Flag::DisableColors, Debug::isTty(), _infoArgs, importer, time);
        Debug{} << "======================== visual color verification end =========================";
    }

    std::ostringstream out;
    Debug redirectOutput{&out};
    CORRADE_VERIFY(Implementation::printInfo(Debug::Flag::DisableColors, false, _infoArgs, importer, time) == false);
    CORRADE_COMPARE_AS(out.str(),
        Utility::Path::join(SCENETOOLS_TEST_DIR, "SceneConverterTestFiles/info-references.txt"),
        TestSuite::Compare::StringToFile);
}

void SceneConverterTest::infoImplementationError() {
    struct Importer: Trade::AbstractImporter {
        Trade::ImporterFeatures doFeatures() const override { return {}; }
        bool doIsOpened() const override { return true; }
        void doClose() override {}

        /* The one single object is named, and that name should be printed
           after all error messages */
        UnsignedLong doObjectCount() const override { return 1; }
        Containers::String doObjectName(UnsignedLong) override { return "A name"; }

        UnsignedInt doSceneCount() const override { return 2; }
        Containers::Optional<Trade::SceneData> doScene(UnsignedInt id) override {
            Error{} << "Scene" << id << "error!";
            return {};
        }

        UnsignedInt doAnimationCount() const override { return 2; }
        Containers::Optional<Trade::AnimationData> doAnimation(UnsignedInt id) override {
            Error{} << "Animation" << id << "error!";
            return {};
        }

        UnsignedInt doSkin2DCount() const override { return 2; }
        Containers::Optional<Trade::SkinData2D> doSkin2D(UnsignedInt id) override {
            Error{} << "2D skin" << id << "error!";
            return {};
        }

        UnsignedInt doSkin3DCount() const override { return 2; }
        Containers::Optional<Trade::SkinData3D> doSkin3D(UnsignedInt id) override {
            Error{} << "3D skin" << id << "error!";
            return {};
        }

        UnsignedInt doLightCount() const override { return 2; }
        Containers::Optional<Trade::LightData> doLight(UnsignedInt id) override {
            Error{} << "Light" << id << "error!";
            return {};
        }

        UnsignedInt doMaterialCount() const override { return 2; }
        Containers::Optional<Trade::MaterialData> doMaterial(UnsignedInt id) override {
            Error{} << "Material" << id << "error!";
            return {};
        }

        UnsignedInt doMeshCount() const override { return 2; }
        Containers::Optional<Trade::MeshData> doMesh(UnsignedInt id, UnsignedInt) override {
            Error{} << "Mesh" << id << "error!";
            return {};
        }

        UnsignedInt doTextureCount() const override { return 2; }
        Containers::Optional<Trade::TextureData> doTexture(UnsignedInt id) override {
            Error{} << "Texture" << id << "error!";
            return {};
        }

        /* Errors for all image types tested in ImageConverterTest */
        UnsignedInt doImage2DCount() const override { return 2; }
        Containers::Optional<Trade::ImageData2D> doImage2D(UnsignedInt id, UnsignedInt) override {
            Error{} << "Image" << id << "error!";
            return {};
        }
    } importer;

    const char* argv[]{"", "--info"};
    CORRADE_VERIFY(_infoArgs.tryParse(Containers::arraySize(argv), argv));

    std::chrono::high_resolution_clock::duration time;

    std::ostringstream out;
    Debug redirectOutput{&out};
    Error redirectError{&out};
    /* It should return a failure */
    CORRADE_VERIFY(Implementation::printInfo(Debug::Flag::DisableColors, {}, _infoArgs, importer, time) == true);
    CORRADE_COMPARE(out.str(),
        /* It should not exit after first error... */
        "Scene 0 error!\n"
        "Scene 1 error!\n"
        "Animation 0 error!\n"
        "Animation 1 error!\n"
        "2D skin 0 error!\n"
        "2D skin 1 error!\n"
        "3D skin 0 error!\n"
        "3D skin 1 error!\n"
        "Light 0 error!\n"
        "Light 1 error!\n"
        "Material 0 error!\n"
        "Material 1 error!\n"
        "Mesh 0 error!\n"
        "Mesh 1 error!\n"
        "Texture 0 error!\n"
        "Texture 1 error!\n"
        "Image 0 error!\n"
        "Image 1 error!\n"
        /* ... and it should print all info output after the errors */
        "Object 0: A name\n");
}

}}}}

CORRADE_TEST_MAIN(Magnum::SceneTools::Test::SceneConverterTest)