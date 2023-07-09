
#include "Precomp.h"
#include "FileResource.h"

// I probably should find a less brain dead way of doing this. :)

std::string FileResource::readAllText(const std::string& filename)
{
	if (filename == "shaders/Scene.vert")
	{
		return R"(
			struct Input
			{
				uint Flags : AttrFlags;
				float3 Position : AttrPos;
				float2 TexCoord : AttrTexCoordOne;
				float2 TexCoord2 : AttrTexCoordTwo;
				float2 TexCoord3 : AttrTexCoordThree;
				float2 TexCoord4 : AttrTexCoordFour;
				float4 Color : AttrColor;
			};

			struct Output
			{
				float4 pos : SV_Position;
				uint flags : PixelFlags;
				float2 texCoord : PixelTexCoordOne;
				float2 texCoord2 : PixelTexCoordTwo;
				float2 texCoord3 : PixelTexCoordThree;
				float2 texCoord4 : PixelTexCoordFour;
				float4 color : PixelColor;
			};

			cbuffer Uniforms
			{
				float4x4 objectToProjection;
			}

			Output main(Input input)
			{
				Output output;
				output.pos = mul(objectToProjection, float4(input.Position, 1.0));
				output.flags = input.Flags;
				output.texCoord = input.TexCoord;
				output.texCoord2 = input.TexCoord2;
				output.texCoord3 = input.TexCoord3;
				output.texCoord4 = input.TexCoord4;
				output.color = input.Color;
				return output;
			}
		)";
	}
	else if (filename == "shaders/Scene.frag")
	{
		return R"(
			struct Input
			{
				float4 pos : SV_Position;
				uint flags : PixelFlags;
				float2 texCoord : PixelTexCoordOne;
				float2 texCoord2 : PixelTexCoordTwo;
				float2 texCoord3 : PixelTexCoordThree;
				float2 texCoord4 : PixelTexCoordFour;
				float4 color : PixelColor;
			};

			struct Output
			{
				float4 outColor : SV_Target;
			};

			SamplerState samplerTex;
			SamplerState samplerTexLightmap;
			SamplerState samplerTexMacro;
			SamplerState samplerTexDetail;

			Texture2D tex;
			Texture2D texLightmap;
			Texture2D texMacro;
			Texture2D texDetail;

			float4 darkClamp(float4 c)
			{
				// Make all textures a little darker as some of the textures (i.e coronas) never become completely black as they should have
				float cutoff = 3.1/255.0;
				return float4(clamp((c.rgb - cutoff) / (1.0 - cutoff), 0.0, 1.0), c.a);
			}

			float4 textureTex(float2 uv) { return tex.Sample(samplerTex, uv); }
			float4 textureMacro(float2 uv) { return texMacro.Sample(samplerTexMacro, uv); }
			float4 textureLightmap(float2 uv) { return texLightmap.Sample(samplerTexLightmap, uv); }
			float4 textureDetail(float2 uv) { return texDetail.Sample(samplerTexDetail, uv); }

			Output main(Input input)
			{
				Output output;

				output.outColor = darkClamp(textureTex(input.texCoord)) * darkClamp(input.color);

				if ((input.flags & 2) != 0) // Macro texture
				{
					output.outColor *= darkClamp(textureMacro(input.texCoord3));
				}

				if ((input.flags & 1) != 0) // Lightmap
				{
					output.outColor.rgb *= clamp(textureLightmap(input.texCoord2).rgb - 0.03, 0.0, 1.0) * 2.0;
				}

				if ((input.flags & 4) != 0) // Detail texture
				{
					float fadedistance = 380.0f;
					float a = clamp(2.0f - (1.0f / input.pos.w) / fadedistance, 0.0f, 1.0f);
					float4 detailColor = (textureDetail(input.texCoord4) - 0.5) * 0.5 + 1.0;
					output.outColor.rgb = lerp(output.outColor.rgb, output.outColor.rgb * detailColor.rgb, a);
				}
				else if ((input.flags & 8) != 0) // Fog map
				{
					float4 fogcolor = darkClamp(textureDetail(input.texCoord4));
					output.outColor.rgb = fogcolor.rgb + output.outColor.rgb * (1.0 - fogcolor.a);
				}
				else if ((input.flags & 16) != 0) // Fog color
				{
					float4 fogcolor = darkClamp(float4(input.texCoord2, input.texCoord3));
					output.outColor.rgb = fogcolor.rgb + output.outColor.rgb * (1.0 - fogcolor.a);
				}

				#if defined(ALPHATEST)
				if (output.outColor.a < 0.5) discard;
				#endif

				output.outColor = clamp(output.outColor, 0.0, 1.0);
				return output;
			}
		)";
	}
	else if (filename == "shaders/PPStep.vert")
	{
		return R"(
			struct Input
			{
				float2 pos : AttrPos;
			};

			struct Output
			{
				float4 pos : SV_Position;
				float2 texCoord : PixelTexCoord;
			};

			Output main(Input input)
			{
				Output output;
				output.pos = float4(input.pos, 0.0, 1.0);
				output.texCoord = input.pos * 0.5 + 0.5;
				return output;
			}
		)";
	}
	else if (filename == "shaders/Present.frag")
	{
		return R"(
			struct Input
			{
				float4 fragCoord : SV_Position;
				float2 texCoord : PixelTexCoord;
			};

			struct Output
			{
				float4 outColor : SV_Target;
			};

			cbuffer PresentPushConstants
			{
				float InvGamma;
				float Contrast;
				float Saturation;
				float Brightness;
				int GrayFormula;
				int Padding1;
				int Padding2;
				int Padding3;
			}

			SamplerState samplerTex
			{
				Filter = MIN_MAG_MIP_LINEAR;
				AddressU = Clamp;
				AddressV = Clamp;
			};

			SamplerState samplerDither
			{
				Filter = MIN_MAG_MIP_POINT;
				AddressU = Wrap;
				AddressV = Wrap;
			};

			Texture2D tex;
			Texture2D texDither;

			float3 dither(float3 c, float4 FragCoord)
			{
				float2 texSize;
				texDither.GetDimensions(texSize.x, texSize.y);
				float threshold = texDither.Sample(samplerDither, FragCoord.xy / texSize).r;
				return floor(c.rgb * 255.0 + threshold) / 255.0;
			}

			float3 applyGamma(float3 c)
			{
				float3 valgray;
				if (GrayFormula == 0)
				{
					float v = c.r + c.g + c.b;
					valgray = float3(v, v, v) * (1 - Saturation) / 3 + c * Saturation;
				}
				else if (GrayFormula == 2)	// new formula
				{
					float v = pow(dot(pow(c, float3(2.2, 2.2, 2.2)), float3(0.2126, 0.7152, 0.0722)), 1.0/2.2);
					valgray = lerp(float3(v, v, v), c, Saturation);
				}
				else
				{
					float v = dot(c, float3(0.3, 0.56, 0.14));
					valgray = lerp(float3(v, v, v), c, Saturation);
				}
				float3 val = valgray * Contrast - (Contrast - 1.0) * 0.5;
				val += Brightness * 0.5;
				val = pow(max(val, float3(0.0, 0.0, 0.0)), float3(InvGamma, InvGamma, InvGamma));
				return val;
			}
			Output main(Input input)
			{
				Output output;
				output.outColor = float4(dither(applyGamma(tex.Sample(samplerTex, input.texCoord).rgb), input.fragCoord), 1.0f);
				return output;
			}
		)";
	}

	return {};
}
