#include "AssetSystem.hpp"
#include <cgltf/cgltf.h>

#include "DX11/DX11Context.hpp"
#include "DX11/DX11Mesh.hpp"
#include "DX11/DX11Shader.hpp"
#include "DX11/DX11Texture.hpp"

namespace global {
	extern DX11Context* rendererSystem;
}

namespace global 
{
	AssetSystem* assetSystem = nullptr;
}

#pragma region static cgltf strings

static const char* cgltf_buffer_view_type_strings[] = {
	"cgltf_buffer_view_type_invalid",
	"cgltf_buffer_view_type_indices",
	"cgltf_buffer_view_type_vertices",
	"cgltf_buffer_view_type_max_enum"
};

static const char* cgltf_attribute_type_strings[] = {
	"cgltf_attribute_type_invalid",
	"cgltf_attribute_type_position",
	"cgltf_attribute_type_normal",
	"cgltf_attribute_type_tangent",
	"cgltf_attribute_type_texcoord",
	"cgltf_attribute_type_color",
	"cgltf_attribute_type_joints",
	"cgltf_attribute_type_weights",
	"cgltf_attribute_type_custom",
	"cgltf_attribute_type_max_enum"
};

static const char* cgltf_component_type_strings[] = {
	"cgltf_component_type_invalid",
	"cgltf_component_type_r_8", /* BYTE */
	"cgltf_component_type_r_8u", /* UNSIGNED_BYTE */
	"cgltf_component_type_r_16", /* SHORT */
	"cgltf_component_type_r_16u", /* UNSIGNED_SHORT */
	"cgltf_component_type_r_32u", /* UNSIGNED_INT */
	"cgltf_component_type_r_32f", /* FLOAT */
    "cgltf_component_type_max_enum"
};

static const char* cgltf_type_strings[] = {
	"cgltf_type_invalid",
	"cgltf_type_scalar",
	"cgltf_type_vec2",
	"cgltf_type_vec3",
	"cgltf_type_vec4",
	"cgltf_type_mat2",
	"cgltf_type_mat3",
	"cgltf_type_mat4",
	"cgltf_type_max_enum"
};

static const char* cgltf_primitive_type_strings[] = {
	"cgltf_primitive_type_invalid",
	"cgltf_primitive_type_points",
	"cgltf_primitive_type_lines",
	"cgltf_primitive_type_line_loop",
	"cgltf_primitive_type_line_strip",
	"cgltf_primitive_type_triangles",
	"cgltf_primitive_type_triangle_strip",
	"cgltf_primitive_type_triangle_fan",
	"cgltf_primitive_type_max_enum"
};

static const char* cgltf_alpha_mode_strings[] = {
	"cgltf_alpha_mode_opaque",
	"cgltf_alpha_mode_mask",
	"cgltf_alpha_mode_blend",
	"cgltf_alpha_mode_max_enum"
};

static const char* cgltf_animation_path_type_strings[] = {
	"cgltf_animation_path_type_invalid",
	"cgltf_animation_path_type_translation",
	"cgltf_animation_path_type_rotation",
	"cgltf_animation_path_type_scale",
	"cgltf_animation_path_type_weights",
	"cgltf_animation_path_type_max_enum"
};

static const char* cgltf_interpolation_type_strings[] = {
	"cgltf_interpolation_type_linear",
	"cgltf_interpolation_type_step",
	"cgltf_interpolation_type_cubic_spline",
	"cgltf_interpolation_type_max_enum"
};

static const char* cgltf_camera_type_strings[] = {
	"cgltf_camera_type_invalid",
	"cgltf_camera_type_perspective",
	"cgltf_camera_type_orthographic",
	"cgltf_camera_type_max_enum"
};

static const char* cgltf_light_type_strings[] = {
	"cgltf_light_type_invalid",
	"cgltf_light_type_directional",
	"cgltf_light_type_point",
	"cgltf_light_type_spot",
	"cgltf_light_type_max_enum"
};

static const char* cgltf_data_free_method_strings[] = {
	"cgltf_data_free_method_none",
	"cgltf_data_free_method_file_release",
	"cgltf_data_free_method_memory_free",
	"cgltf_data_free_method_max_enum"
};

static const char* cgltf_meshopt_compression_mode_strings[] = {
	"cgltf_meshopt_compression_mode_invalid",
	"cgltf_meshopt_compression_mode_attributes",
	"cgltf_meshopt_compression_mode_triangles",
	"cgltf_meshopt_compression_mode_indices",
	"cgltf_meshopt_compression_mode_max_enum"
};

static const char* cgltf_meshopt_compression_filter_strings[] = {
	"cgltf_meshopt_compression_filter_none",
	"cgltf_meshopt_compression_filter_octahedral",
	"cgltf_meshopt_compression_filter_quaternion",
	"cgltf_meshopt_compression_filter_exponential",
	"cgltf_meshopt_compression_filter_max_enum"
};


static const char* jsmntype_strings[] = {
	"JSMN_UNDEFINED",
	"JSMN_OBJECT",
	"JSMN_ARRAY",
	"JSMN_STRING",
	"JSMN_PRIMITIVE"
};

#pragma endregion

MeshAsset::MeshAsset(std::string_view filePath)
	: m_filePath(filePath)
{
	
}

// @TODO: should this be allowed?
MeshAsset::MeshAsset(
	const std::vector<float3>&& positions,
	const std::vector<float3>&& normals, 
	const std::vector<float3>&& tangents,
	const std::vector<float3>&& colors,
	const std::vector<float2>&& uv0s,
	const std::vector<float2>&& uv1s,
	const std::vector<u32>&& indices)
	// these are copies, cpp and its implicitness!!!!
	: m_positions(positions), m_normals(normals), m_tangents(tangents), m_colors(colors), m_uv0s(uv0s), m_uv1s(uv1s), m_indices(indices)
{

}

void MeshAsset::Load()
{
	// if we dont have a file path then we set the verted data ourselves
	if (!m_filePath.empty()) 
	{
		std::string realPath = global::assetSystem->GetRealPath(m_filePath);
		// @TODO: temprary, gltf loader should make meshes and stuff instead, only processing should happen here? or is that file a custom mesh format?
		spdlog::info("loading mesh {}", realPath);

		// @TODO: customise options
		cgltf_options options = {};
		cgltf_data* data = nullptr;
		cgltf_result result = cgltf_parse_file(&options, realPath.data(), &data);

		if (result != cgltf_result_success) {
			spdlog::error("failed loading mesh {}", realPath);
			return;
		}

		spdlog::info("loaded mesh {}", realPath);

		spdlog::info("{}", data->json);
		GltfPrintInfo(data);

		if(cgltf_load_buffers(&options, data, realPath.data()) != cgltf_result_success) {
			spdlog::error("failed loading mesh buffers {}", realPath);
			return;
		}

		spdlog::info("loaded mesh buffers {}", realPath);

		ENSURE(data->meshes_count > 0, "");
		cgltf_mesh* mesh = &data->meshes[0];

		ENSURE(mesh->primitives_count > 0, "");
		cgltf_primitive* primitive = &mesh->primitives[0];

		// get indices
		{
			cgltf_size count = cgltf_accessor_unpack_indices(primitive->indices, nullptr, sizeof(u32), 0);
			m_indices.resize(count);
			count = cgltf_accessor_unpack_indices(primitive->indices, m_indices.data(), sizeof(u32), m_indices.size());
		}

		for(int a = 0; a < mesh->primitives->attributes_count; ++a) {
			cgltf_attribute* attribute = &mesh->primitives->attributes[a];
			cgltf_buffer_view* buffer_view = attribute->data->buffer_view;

			ENSURE(buffer_view->type == cgltf_buffer_view_type_vertices, "");

			switch(attribute->type) {
			case cgltf_attribute_type_position: {
				// cgltf_size count = cgltf_accessor_unpack_floats(attribute->data, nullptr, 0);
				m_positions.resize(attribute->data->count);
				(void)cgltf_accessor_unpack_floats(attribute->data, reinterpret_cast<cgltf_float*>(m_positions.data()), 3 * m_positions.size());
			} break;
			case cgltf_attribute_type_normal: {
				// cgltf_size count = cgltf_accessor_unpack_floats(attribute->data, nullptr, 0);
				m_normals.resize(attribute->data->count);
				(void)cgltf_accessor_unpack_floats(attribute->data, reinterpret_cast<cgltf_float*>(m_normals.data()), 3 * m_normals.size());
			} break;
			case cgltf_attribute_type_tangent: {
				// cgltf_size count = cgltf_accessor_unpack_floats(attribute->data, nullptr, 0);
				m_tangents.resize(attribute->data->count);
				(void)cgltf_accessor_unpack_floats(attribute->data, reinterpret_cast<cgltf_float*>(m_tangents.data()), 3 * m_tangents.size());
			} break;
			case cgltf_attribute_type_texcoord: {
				if(attribute->index == 0) {
					// cgltf_size count = cgltf_accessor_unpack_floats(attribute->data, nullptr, 0);
					m_uv0s.resize(attribute->data->count);
					(void)cgltf_accessor_unpack_floats(attribute->data, reinterpret_cast<cgltf_float*>(m_uv0s.data()), 2 * m_uv0s.size());
				}

				if(attribute->index == 1) {
					// cgltf_size count = cgltf_accessor_unpack_floats(attribute->data, nullptr, 0);
					m_uv1s.resize(attribute->data->count);
					(void)cgltf_accessor_unpack_floats(attribute->data, reinterpret_cast<cgltf_float*>(m_uv1s.data()), 2 * m_uv1s.size());
				}
			} break;
			case cgltf_attribute_type_color: {
				// cgltf_size count = cgltf_accessor_unpack_floats(attribute->data, nullptr, 0);
				m_colors.resize(attribute->data->count);
				(void)cgltf_accessor_unpack_floats(attribute->data, reinterpret_cast<cgltf_float*>(m_colors.data()), 3 * m_colors.size());
			} break;
			case cgltf_attribute_type_joints: {

			} break;
			case cgltf_attribute_type_weights: {

			} break;
			case cgltf_attribute_type_custom: {

			} break;
			case cgltf_attribute_type_invalid: 
			case cgltf_attribute_type_max_enum: {
				UNREACHABLE("");
			} break;
			};
		}

		// @TODO: this below is a hack, do better

		// ensure normals exist
		if(m_positions.size() != m_normals.size()) {
			m_normals.resize(m_positions.size());
			// @TODO: calculate normals
		}

		// ensure colors exist
		if(m_positions.size() != m_colors.size()) {
			m_colors.resize(m_positions.size());
			memset(m_colors.data(), 0, m_colors.size() * sizeof(float3));
		}

		// ensure uv0s exist
		if(m_positions.size() != m_uv0s.size()) {
			m_uv0s.resize(m_positions.size());
			memset(m_uv0s.data(), 0, m_uv0s.size() * sizeof(float2));
		}

		ENSURE(m_positions.size() == m_normals.size(), "");
		// ENSURE(m_positions.size() == m_tangents.size(), "");
		ENSURE(m_positions.size() == m_colors.size(), "");
		ENSURE(m_positions.size() == m_uv0s.size(), "");
		// ENSURE(m_positions.size() == m_uv1s.size(), "");

		cgltf_free(data);
		spdlog::info("processed mesh {}", realPath);
	}

	state = AssetState::Loaded;

	InitRendererResource();
}

void MeshAsset::Unload()
{
	m_indices.clear();
	
	m_positions.clear();
	m_normals.clear();
	m_tangents.clear();
	m_colors.clear();
	m_uv0s.clear();
	m_uv1s.clear();

	state = AssetState::Unloaded;
}

void* MeshAsset::GetRendererResource() const
{
	return m_rendererResource;
}

void MeshAsset::InitRendererResource()
{
	DX11Mesh::CreateInfo createInfo = {
		.attributesCount = m_positions.size(),
		
		.positions = m_positions.data(),
		.normals = m_normals.data(),
		.tangents = m_tangents.data(),
		.colors = m_colors.data(),
		.uv0s = m_uv0s.data(),
		.uv1s = m_uv1s.data(),
		
		.indicesCount = m_indices.size(),
		.indices = m_indices.data(),
	};
	m_rendererResource = new DX11Mesh(global::rendererSystem->GetDevice(), createInfo);
}

#pragma region debug print gltf file

void MeshAsset::GltfPrintInfo(cgltf_data* data) {
	for(int s = 0;s < data->scenes_count; ++s) {
		cgltf_scene scene = data->scenes[s];
		spdlog::info("[scene name={} nodes_count={}]", SPDLOG_PTR(scene.name), scene.nodes_count);
	}
	
	GltfPrintMeshInfo(data);

	GltfPrintMaterialInfo(data);

	GltfPrintImageInfo(data);

	GltfPrintAnimationInfo(data);

	for(int a = 0; a < data->accessors_count; ++a) {
		cgltf_accessor accessor = data->accessors[a];
		spdlog::info("[accessor name={} comp_type={} is_sparse={}]", SPDLOG_PTR(accessor.name), SPDLOG_PTR(cgltf_component_type_strings[accessor.component_type]), accessor.is_sparse);
	}
}


void MeshAsset::GltfPrintAnimationInfo(cgltf_data* data) {
	for(int a = 0; a < data->animations_count; ++a) {
		cgltf_animation animation = data->animations[a];
		spdlog::info("[animation name={}]", SPDLOG_PTR(animation.name));

		for(int c = 0; c < animation.channels_count; ++c) {
			spdlog::info("[channel path_type={}]", SPDLOG_PTR(cgltf_animation_path_type_strings[animation.channels[c].target_path]));
		}
	}
}

void MeshAsset::GltfPrintMaterialInfo(cgltf_data* data) {
	for(int m = 0;m < data->materials_count; ++m) {
		cgltf_material material = data->materials[m];

		spdlog::info("[material name={}]", SPDLOG_PTR(material.name));
	}
}

void MeshAsset::GltfPrintImageInfo(cgltf_data* data) {
	for(int i = 0;i < data->images_count; ++i) {
		cgltf_image image = data->images[i];
		spdlog::info("[image name={} uri={} mime_type={}]", SPDLOG_PTR(image.name), SPDLOG_PTR(image.uri), SPDLOG_PTR(image.mime_type));
	}	
}

void MeshAsset::GltfPrintMeshInfo(cgltf_data* data) {

	for(int m = 0;m < data->meshes_count; ++m) {
		cgltf_mesh mesh = data->meshes[m];
		spdlog::info("[mesh name={}]", SPDLOG_PTR(mesh.name));
		
		for(int p = 0;p < mesh.primitives_count; ++p) {
			cgltf_primitive primitive = mesh.primitives[p];
			spdlog::info("[primitive type={}]", SPDLOG_PTR(cgltf_primitive_type_strings[primitive.type]));

			for(int a = 0; a < primitive.attributes_count; ++a) {
				cgltf_attribute attribute = primitive.attributes[a];
				spdlog::info("[attribute name={} index={} type={}]", SPDLOG_PTR(attribute.name), attribute.index, SPDLOG_PTR(cgltf_attribute_type_strings[attribute.type]));
			}

			spdlog::info("[indices name={} type={}]", SPDLOG_PTR(primitive.indices->name), SPDLOG_PTR(cgltf_component_type_strings[primitive.indices->component_type]));
		}

		for(int w = 0;w < mesh.weights_count; ++w) {
			cgltf_float weight = mesh.weights[w];
			spdlog::info("[weight value={}]", weight);
		}

		for(int tn = 0; tn < mesh.target_names_count; ++tn) {
			const char* tname = mesh.target_names[tn];
			spdlog::info("[target name={}]", SPDLOG_PTR(tname));
		}

		for(int e = 0;e < mesh.extensions_count; ++e) {
			cgltf_extension extension = mesh.extensions[e];
			spdlog::info("[extension name={}]", SPDLOG_PTR(extension.name));
		}
	}
}

#pragma endregion debug print gltf file

TextureAsset::TextureAsset(std::string_view filePath) 
	: m_filePath(filePath)
{
}

TextureAsset::~TextureAsset() 
{
}

void TextureAsset::Load()
{
	std::string realPath = global::assetSystem->GetRealPath(m_filePath);
	m_data = stbi_load(realPath.data(), &m_width, &m_height, &m_numComponents, 4);
	InitRendererResource();

	state = AssetState::Loaded;
}

void TextureAsset::Unload()
{
	stbi_image_free(m_data);
}

void* TextureAsset::GetRendererResource() const
{
	return m_rendererResource;
}

void TextureAsset::InitRendererResource()
{
	DX11Texture::CreateInfo createInfo = {
		.width = m_width,
		.height = m_height,
		.numComponents = m_numComponents,
		.data = m_data,
	};
	m_rendererResource = new DX11Texture(global::rendererSystem->GetDevice(), createInfo);
}

void ShaderAsset::Load()
{
	InitRendererResource();
	state = AssetState::Loaded;
}

void ShaderAsset::Unload()
{
}

void* ShaderAsset::GetRendererResource() const
{
	return m_rendererResource;
}

void ShaderAsset::InitRendererResource()
{
	auto device = global::rendererSystem->GetDevice();
	switch (m_kind)
	{
	case ShaderAsset::Kind::Invalid:
		ENSURE(false, "");
		break;
	case ShaderAsset::Kind::Vertex:
		m_rendererResource = new DX11VertexShader(device, DX11VertexShader::CreateInfo{
				.blob = blob,
				.blobSize = blobSize,
			});
		break;
	case ShaderAsset::Kind::Pixel:
		m_rendererResource = new DX11PixelShader(device, DX11PixelShader::CreateInfo{
				.blob = blob,
				.blobSize = blobSize, 
			});
		break;
	default:
		ENSURE(false, "");
		break;
	}
}

void AssetSystem::RegisterAssets()
{
	// engine meshes, used by engine systems like the renderer
	{
		std::vector<float3> m_quadMeshPositions = {
			float3(-1.0f, -1.0f, 0.0f),
			float3(1.0f, 1.0f, 0.0f),
			float3(1.0f, -1.0f, 0.0f),
			float3(-1.0f, 1.0f, 0.0f)
		};
		std::vector<float2> m_quadMeshUv0s = {
			float2(0.0f, 1.0f),
			float2(1.0f, 0.0f),
			float2(1.0f, 1.0f),
			float2(0.0f, 0.0f),
		};
		std::vector<u32> m_quadMeshIndices = {
			0, 1, 2,
			0, 3, 1
		};

		MeshID id = m_catalog->RegisterMeshAsset(MeshAsset(
			std::move(m_quadMeshPositions),
			{/* normal */}, 
			{/* tangent */}, 
			{/* color */}, 
			std::move(m_quadMeshUv0s),
			{/* uv1 */}, 
			std::move(m_quadMeshIndices)
		));
		MeshAsset& asset = const_cast<MeshAsset&>(m_catalog->GetMeshAsset(id));
		asset.Load();
	}

	// @TODO: upload thr render resources of the assets after the renderer is initialised
	// @TODO: currently we dont unload anything, there needs to be a system that decides on scene transition, 
	// or something more dynamic that loads and unloads resources from disk
	{
		// MeshID id = m_catalog->RegisterMeshAsset(MeshAsset("meshes/quad.glb"));
		// MeshAsset& asset = const_cast<MeshAsset&>(m_catalog->GetMeshAsset(id));
		// asset.Load();
	}

	{
		MeshID id = m_catalog->RegisterMeshAsset(MeshAsset("meshes/suzanne.glb"));
		MeshAsset& asset = const_cast<MeshAsset&>(m_catalog->GetMeshAsset(id));
		asset.Load();
	}

	{
		MeshID id = m_catalog->RegisterMeshAsset(MeshAsset("meshes/two_cubes.glb"));
		MeshAsset& asset = const_cast<MeshAsset&>(m_catalog->GetMeshAsset(id));
		asset.Load();
	}

	{
		MeshID id = m_catalog->RegisterMeshAsset(MeshAsset("meshes/scene1.glb"));
		MeshAsset& asset = const_cast<MeshAsset&>(m_catalog->GetMeshAsset(id));
		asset.Load();
	}

	{
		// 0
		ShaderID id = m_catalog->RegisterShaderAsset(ShaderAsset(ShaderAsset::Kind::Vertex, L"shaders/simple_vs.hlsl", "VSMain", "vs_5_0"));
		global::rendererSystem->shaderCompiler->CompileShaderAsset(id);
		ShaderAsset& asset = const_cast<ShaderAsset&>(m_catalog->GetShaderAsset(id));
		asset.Load();
	}

	{
		// 1
		ShaderID id = m_catalog->RegisterShaderAsset(ShaderAsset(ShaderAsset::Kind::Pixel, L"shaders/simple_ps.hlsl", "PSMain", "ps_5_0"));
		global::rendererSystem->shaderCompiler->CompileShaderAsset(id);
		ShaderAsset& asset = const_cast<ShaderAsset&>(m_catalog->GetShaderAsset(id));
		asset.Load();
	}

	{
		// 2
		ShaderID id = m_catalog->RegisterShaderAsset(ShaderAsset(ShaderAsset::Kind::Vertex, L"shaders/simple_deferred_vs.hlsl", "VSMain", "vs_5_0"));
		global::rendererSystem->shaderCompiler->CompileShaderAsset(id);
		ShaderAsset& asset = const_cast<ShaderAsset&>(m_catalog->GetShaderAsset(id));
		asset.Load();
	}

	{
		// 3
		ShaderID id = m_catalog->RegisterShaderAsset(ShaderAsset(ShaderAsset::Kind::Pixel, L"shaders/simple_deferred_ps.hlsl", "PSMain", "ps_5_0"));
		global::rendererSystem->shaderCompiler->CompileShaderAsset(id);
		ShaderAsset& asset = const_cast<ShaderAsset&>(m_catalog->GetShaderAsset(id));
		asset.Load();
	}

	{
		// 4
		ShaderID id = m_catalog->RegisterShaderAsset(ShaderAsset(ShaderAsset::Kind::Vertex, L"shaders/final_deferred_pass_vs.hlsl", "VSMain", "vs_5_0"));
		global::rendererSystem->shaderCompiler->CompileShaderAsset(id);
		ShaderAsset& asset = const_cast<ShaderAsset&>(m_catalog->GetShaderAsset(id));
		asset.Load();
	}

	{
		// 5
		ShaderID id = m_catalog->RegisterShaderAsset(ShaderAsset(ShaderAsset::Kind::Pixel, L"shaders/final_deferred_pass_ps.hlsl", "PSMain", "ps_5_0"));
		global::rendererSystem->shaderCompiler->CompileShaderAsset(id);
		ShaderAsset& asset = const_cast<ShaderAsset&>(m_catalog->GetShaderAsset(id));
		asset.Load();
	}

	{
		TextureID id = m_catalog->RegisterTextureAsset(TextureAsset("textures/checker.png"));
		TextureAsset& asset = const_cast<TextureAsset&>(m_catalog->GetTextureAsset(id));
		asset.Load();
	}
}
