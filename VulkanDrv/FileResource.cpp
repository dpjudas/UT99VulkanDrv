
#include "Precomp.h"
#include "FileResource.h"

// I probably should find a less brain dead way of doing this. :)

std::string FileResource::readAllText(const std::string& filename)
{
	if (filename == "shaders/Scene.vert")
	{
		return R"(
			layout(push_constant) uniform ScenePushConstants
			{
				mat4 objectToProjection;
			};

			layout(location = 0) in uint aFlags;
			layout(location = 1) in vec3 aPosition;
			layout(location = 2) in vec2 aTexCoord;
			layout(location = 3) in vec2 aTexCoord2;
			layout(location = 4) in vec2 aTexCoord3;
			layout(location = 5) in vec2 aTexCoord4;
			layout(location = 6) in vec4 aColor;

			layout(location = 0) flat out uint flags;
			layout(location = 1) out vec2 texCoord;
			layout(location = 2) out vec2 texCoord2;
			layout(location = 3) out vec2 texCoord3;
			layout(location = 4) out vec2 texCoord4;
			layout(location = 5) out vec4 color;

			void main()
			{
				gl_Position = objectToProjection * vec4(aPosition, 1.0);
				flags = aFlags;
				texCoord = aTexCoord;
				texCoord2 = aTexCoord2;
				texCoord3 = aTexCoord3;
				texCoord4 = aTexCoord4;
				color = aColor;
			}
		)";
	}
	else if (filename == "shaders/Scene.frag")
	{
		return R"(
			layout(binding = 0) uniform sampler2D tex;
			layout(binding = 1) uniform sampler2D texLightmap;
			layout(binding = 2) uniform sampler2D texMacro;
			layout(binding = 3) uniform sampler2D texDetail;

			layout(location = 0) flat in uint flags;
			layout(location = 1) in vec2 texCoord;
			layout(location = 2) in vec2 texCoord2;
			layout(location = 3) in vec2 texCoord3;
			layout(location = 4) in vec2 texCoord4;
			layout(location = 5) in vec4 color;

			layout(location = 0) out vec4 outColor;

			vec3 linear(vec3 c)
			{
				return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(c, vec3(0.04045)));
			}

			void main()
			{
				outColor = texture(tex, texCoord);
				//outColor.rgb = linear(outColor.rgb);

				if ((flags & 2) != 0) // Macro texture
				{
					outColor *= texture(texMacro, texCoord3);
				}

				if ((flags & 1) != 0) // Lightmap
				{
					outColor *= texture(texLightmap, texCoord2);
				}

				if ((flags & 4) != 0) // Detail texture
				{
					/* To do: apply fade out: Min( appRound(100.f * (NearZ / Poly->Pts[i]->Point.Z - 1.f)), 255) */

					float detailScale = 1.0;
					for (int i = 0; i < 3; i++)
					{
						outColor *= texture(texDetail, texCoord4 * detailScale) + 0.5;
						detailScale *= 4.223f;
					}
				}
				else if ((flags & 8) != 0) // Fog map
				{
					vec4 fogcolor = texture(texDetail, texCoord4);
					outColor = fogcolor + outColor * (1.0 - fogcolor.a);
				}

				outColor *= color;

				#if defined(ALPHATEST)
				if (outColor.a < 0.5) discard;
				#endif
			}
		)";
	}
	else if (filename == "shaders/PPStep.vert")
	{
		return R"(
			layout(location = 0) out vec2 texCoord;

			vec2 positions[6] = vec2[](
				vec2(-1.0, -1.0),
				vec2( 1.0, -1.0),
				vec2(-1.0,  1.0),
				vec2(-1.0,  1.0),
				vec2( 1.0, -1.0),
				vec2( 1.0,  1.0)
			);

			void main()
			{
				vec4 pos = vec4(positions[gl_VertexIndex], 0.0, 1.0);
				gl_Position = pos;
				texCoord = pos.xy * 0.5 + 0.5;
			}
		)";
	}
	else if (filename == "shaders/BloomCombine.frag")
	{
		return R"(
			layout(location = 0) in vec2 texCoord;
			layout(location = 0) out vec4 FragColor;
			layout(binding = 0) uniform sampler2D bloom;

			void main()
			{
				FragColor = vec4(texture(bloom, texCoord).rgb, 0.0);
			}
		)";
	}
	else if (filename == "shaders/BloomExtract.frag")
	{
		return R"(
			layout(location = 0) in vec2 texCoord;
			layout(location = 0) out vec4 FragColor;
			layout(binding = 0) uniform sampler2D sceneTexture;
			layout(binding = 1) uniform sampler2D exposureTexture;

			void main()
			{
				float exposureAdjustment = texture(exposureTexture, vec2(0.5)).x;
				vec4 color = texture(sceneTexture, Offset + texCoord * Scale);
				FragColor = max(vec4((color.rgb + vec3(0.001)) * exposureAdjustment - 1, 1), vec4(0));
			}
		)";
	}
	else if (filename == "shaders/Blur.frag")
	{
		return R"(
			layout(location = 0) in vec2 texCoord;
			layout(location = 0) out vec4 FragColor;
			layout(binding = 0) uniform sampler2D SourceTexture;

			void main()
			{
			#if defined(BLUR_HORIZONTAL)
				FragColor =
					textureOffset(SourceTexture, texCoord, ivec2( 0, 0)) * SampleWeights0 +
					textureOffset(SourceTexture, texCoord, ivec2( 1, 0)) * SampleWeights1 +
					textureOffset(SourceTexture, texCoord, ivec2(-1, 0)) * SampleWeights2 +
					textureOffset(SourceTexture, texCoord, ivec2( 2, 0)) * SampleWeights3 +
					textureOffset(SourceTexture, texCoord, ivec2(-2, 0)) * SampleWeights4 +
					textureOffset(SourceTexture, texCoord, ivec2( 3, 0)) * SampleWeights5 +
					textureOffset(SourceTexture, texCoord, ivec2(-3, 0)) * SampleWeights6;
			#else
				FragColor =
					textureOffset(SourceTexture, texCoord, ivec2(0, 0)) * SampleWeights0 +
					textureOffset(SourceTexture, texCoord, ivec2(0, 1)) * SampleWeights1 +
					textureOffset(SourceTexture, texCoord, ivec2(0,-1)) * SampleWeights2 +
					textureOffset(SourceTexture, texCoord, ivec2(0, 2)) * SampleWeights3 +
					textureOffset(SourceTexture, texCoord, ivec2(0,-2)) * SampleWeights4 +
					textureOffset(SourceTexture, texCoord, ivec2(0, 3)) * SampleWeights5 +
					textureOffset(SourceTexture, texCoord, ivec2(0,-3)) * SampleWeights6;
			#endif
			}
		)";
	}
	else if (filename == "shaders/ExposureExtract.frag")
	{
		return R"(
			layout(location=0) in vec2 TexCoord;
			layout(location=0) out vec4 FragColor;

			layout(binding=0) uniform sampler2D SceneTexture;

			void main()
			{
				vec4 color = texture(SceneTexture, Offset + TexCoord * Scale);
				FragColor = vec4(max(max(color.r, color.g), color.b), 0.0, 0.0, 1.0);
			}
		)";
	}
	else if (filename == "shaders/ExposureAverage.frag")
	{
		return R"(
			layout(location=0) in vec2 TexCoord;
			layout(location=0) out vec4 FragColor;
			layout(binding=0) uniform sampler2D ExposureTexture;

			void main()
			{
				vec4 values = textureGather(ExposureTexture, TexCoord);
				FragColor = vec4((values.x + values.y + values.z + values.w) * 0.25, 0.0, 0.0, 1.0);
			}
		)";
	}
	else if (filename == "shaders/ExposureCombine.frag")
	{
		return R"(
			layout(location=0) in vec2 TexCoord;
			layout(location=0) out vec4 FragColor;

			layout(binding=0) uniform sampler2D ExposureTexture;

			void main()
			{
				float light = texture(ExposureTexture, TexCoord).x;
				float exposureAdjustment = 1.0 / max(ExposureBase + light * ExposureScale, ExposureMin);
				FragColor = vec4(exposureAdjustment, 0.0, 0.0, ExposureSpeed);
			}
		)";
	}
	else if (filename == "shaders/Tonemap.frag")
	{
		return R"(
			layout(location = 0) in vec2 texCoord;
			layout(location = 0) out vec4 FragColor;
			layout(binding = 0) uniform sampler2D scene;

			vec3 uncharted2Tonemap(vec3 x)
			{
				float A = 0.15;
				float B = 0.50;
				float C = 0.10;
				float D = 0.20;
				float E = 0.02;
				float F = 0.30;
				return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
			}

			vec3 sRGB(vec3 c)
			{
				c = max(c, vec3(0.0));
				return mix(1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, 12.92 * c, step(c, vec3(0.0031308)));
				//return pow(c, vec3(1.0 / 2.2)); // cheaper, but assuming gamma of 2.2
				//return sqrt(c); // cheaper, but assuming gamma of 2.0
			}

			vec3 linear(vec3 c)
			{
				return mix(pow((c + 0.055) / 1.055, vec3(2.4)), c / 12.92, step(c, vec3(0.04045)));
			}

			void main()
			{
/*
				vec3 color = texture(scene, texCoord).rgb;

				float W = 11.2;
				vec3 curr = uncharted2Tonemap(color);
				vec3 whiteScale = vec3(1) / uncharted2Tonemap(vec3(W));
				FragColor = vec4(sRGB(curr * whiteScale), 0.0);
*/
				FragColor = vec4(sRGB(texture(scene, texCoord).rgb), 0.0);
			}
		)";
	}
	else if (filename == "shaders/Present.frag")
	{
		return R"(
			layout(binding = 0) uniform sampler2D texSampler;
			layout(location = 0) in vec2 texCoord;
			layout(location = 0) out vec4 outColor;

			void main()
			{
				outColor = texture(texSampler, texCoord);
			}
		)";
	}

	return {};
}
