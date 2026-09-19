struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

cbuffer ImuxUiConstants : register(b0) {
    float4 accent;
    float time;
};

float4 main(PSInput input) : SV_TARGET {
    float wave = 0.5 + 0.5 * sin(input.uv.x * 8.0 + time);
    float glow = 0.035 * wave;
    return float4(accent.rgb + glow, 1.0);
}
