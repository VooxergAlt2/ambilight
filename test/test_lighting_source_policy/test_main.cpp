#include <cstdint>
#include <unity.h>
#include "render/LightingSourcePolicy.h"
using ambilight::LightingOwner;
using ambilight::LightingSourcePolicy;
using ambilight::ManualLightingEffect;
using ambilight::ManualLightingState;

void test_auto_uses_local_before_first_ddp_frame() {
    ManualLightingState state;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(LightingOwner::Local), static_cast<int>(LightingSourcePolicy::resolve(state, 1000000ULL, 0)));
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ManualLightingEffect::BiasWhite), static_cast<std::uint8_t>(LightingSourcePolicy::localEffect(state)));
}
void test_auto_uses_ddp_while_stream_is_fresh() {
    ManualLightingState state;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(LightingOwner::Ddp), static_cast<int>(LightingSourcePolicy::resolve(state, 2000000ULL, 1000000ULL)));
}
void test_auto_keeps_ddp_at_exact_timeout_boundary() {
    ManualLightingState state;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(LightingOwner::Ddp), static_cast<int>(LightingSourcePolicy::resolve(state, 2500000ULL, 1000000ULL)));
}
void test_auto_falls_back_after_timeout() {
    ManualLightingState state;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(LightingOwner::Local), static_cast<int>(LightingSourcePolicy::resolve(state, 2500001ULL, 1000000ULL)));
}
void test_explicit_local_effect_overrides_fresh_ddp() {
    ManualLightingState state;
    state.effect = ManualLightingEffect::Aurora;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(LightingOwner::Local), static_cast<int>(LightingSourcePolicy::resolve(state, 1200000ULL, 1199999ULL)));
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ManualLightingEffect::Aurora), static_cast<std::uint8_t>(LightingSourcePolicy::localEffect(state)));
}
void test_invalid_fallback_is_sanitized_to_bias_white() {
    ManualLightingState state;
    state.effect = ManualLightingEffect::Ambilight;
    state.fallbackEffect = ManualLightingEffect::Ambilight;
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ManualLightingEffect::BiasWhite), static_cast<std::uint8_t>(LightingSourcePolicy::localEffect(state)));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_auto_uses_local_before_first_ddp_frame);
    RUN_TEST(test_auto_uses_ddp_while_stream_is_fresh);
    RUN_TEST(test_auto_keeps_ddp_at_exact_timeout_boundary);
    RUN_TEST(test_auto_falls_back_after_timeout);
    RUN_TEST(test_explicit_local_effect_overrides_fresh_ddp);
    RUN_TEST(test_invalid_fallback_is_sanitized_to_bias_white);
    return UNITY_END();
}
