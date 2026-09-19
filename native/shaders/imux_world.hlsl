cbuffer Camera : register(b0) {
    matrix worldViewProjection;
    float time;
    float3 cameraPosition;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float2 uv : TEXCOORD1;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float3 worldPosition : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.position = mul(float4(input.position, 1.0), worldViewProjection);
    output.normal = input.normal;
    output.color = input.color;
    output.worldPosition = input.position;
    output.uv = input.uv;
    return output;
}

Texture2D blockTexture : register(t0);
SamplerState blockSampler : register(s0);

float4 PSMain(VSOutput input) : SV_TARGET {
    float3 lightDirection = normalize(float3(-0.45, 0.85, -0.35));
    float diffuse = saturate(dot(normalize(input.normal), lightDirection)) * 0.65 + 0.35;
    float pulse = 0.025 * sin(time * 1.7 + input.worldPosition.x * 0.15);
    float3 texel = blockTexture.Sample(blockSampler, input.uv).rgb;
    float3 lit = texel * input.color.rgb * (diffuse + pulse);
    float distanceToCamera = distance(input.worldPosition, cameraPosition);
    float fog = saturate((distanceToCamera - 28.0) / 52.0);
    float3 atmospheric = float3(0.10, 0.18, 0.22);
    lit = lerp(lit, atmospheric, fog * 0.58);
    float edge = pow(1.0 - saturate(dot(normalize(input.normal), normalize(float3(0.2, 0.5, -0.7)))), 3.0);
    lit += edge * float3(0.04, 0.11, 0.09);
    return float4(lit, 1.0);
}
