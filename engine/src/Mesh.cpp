#include "Mesh.hpp"

#include <cgltf/cgltf.h>


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
	// @TODO: customise options
	cgltf_options options = {};
	cgltf_data* data = nullptr;
	cgltf_result result = cgltf_parse_file(&options, filePath.data(), &data);

	if (result != cgltf_result_success) {
		return;
	}

	// spdlog::info("{}", data->json);
	
	GltfPrintInfo(data);

	if(cgltf_load_buffers(&options, data, filePath.data()) != cgltf_result_success) {
		return;
	}




	// @TODO: temprary
	cgltf_mesh* mesh = &data->meshes[0];
	cgltf_primitive* primitive = &mesh->primitives[0];

	// cgltf_accessor_unpack_indices(primitive->indices, nullptr, );
	ASSERT(primitive->indices->component_type == cgltf_component_type::cgltf_component_type_r_16u, "need u16 indices");

	// data->accessors[0].

	cgltf_size indices_offset = primitive->indices->buffer_view->offset;
	u16* indices_buffer = reinterpret_cast<u16*>(static_cast<byte*>(primitive->indices->buffer_view->buffer->data) + indices_offset);
	cgltf_size indices_count = primitive->indices->count;

	m_indices.reserve(indices_count);
	for(int i = 0;i < indices_count; ++i) {
		u16 index = indices_buffer[i];
		m_indices.push_back(index);
	}

	// m_vertices.reserve();

	const byte* positions = nullptr;
	cgltf_size positions_count = 0;

	const byte* colors = nullptr;
	cgltf_size colors_count = 0;

	const byte* uvs = nullptr;
	cgltf_size uvs_count = 0;

	const byte* normals = nullptr;
	cgltf_size normals_count = 0;

	const byte* tangents = nullptr;
	cgltf_size tangents_count = 0;

	for(int a = 0; a < mesh->primitives->attributes_count; ++a) {
		cgltf_attribute* attribute = &mesh->primitives->attributes[a];
		cgltf_buffer_view* buffer_view = attribute->data->buffer_view;

		ASSERT(buffer_view->type == cgltf_buffer_view_type_vertices, "");

		switch(attribute->type) {
		case cgltf_attribute_type_position: {
			ASSERT(attribute->data->type == cgltf_type_vec3, "");
			ASSERT(attribute->data->component_type == cgltf_component_type_r_32f, "")

			positions_count = attribute->data->count;
			positions = cgltf_buffer_view_data(buffer_view);
		} break;
		case cgltf_attribute_type_normal: {
			ASSERT(attribute->data->type == cgltf_type_vec3, "");
			ASSERT(attribute->data->component_type == cgltf_component_type_r_32f, "")
			
			normals_count = attribute->data->count;
			normals = cgltf_buffer_view_data(buffer_view);
		} break;
		case cgltf_attribute_type_tangent: {
			ASSERT(attribute->data->type == cgltf_type_vec3, "");
			ASSERT(attribute->data->component_type == cgltf_component_type_r_32f, "")
			
			tangents_count = attribute->data->count;
			tangents = cgltf_buffer_view_data(buffer_view);
		} break;
		case cgltf_attribute_type_texcoord: {
			ASSERT(attribute->data->type == cgltf_type_vec2, "");
			ASSERT(attribute->data->component_type == cgltf_component_type_r_32f, "")
			
			uvs_count = attribute->data->count;
			uvs = cgltf_buffer_view_data(buffer_view);
		} break;
		case cgltf_attribute_type_color: {
			ASSERT(attribute->data->type == cgltf_type_vec3, "");
			ASSERT(attribute->data->component_type == cgltf_component_type_r_32f, "")
			
			colors_count = attribute->data->count;
			colors = cgltf_buffer_view_data(buffer_view);
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

	ASSERT(positions_count == colors_count, "");
	ASSERT(positions_count == uvs_count, "");
	ASSERT(positions_count == normals_count, "");
	ASSERT(positions_count == tangents_count, "");	

	cgltf_free(data);
}

MeshAsset::MeshAsset(const std::vector<Vertex>& vertices, const std::vector<u32>& indices) 
	: m_vertices(vertices), m_indices(indices)
{
	
}


void Mesh::CreateBuffers()
{
    if(auto ma = m_meshAsset.lock(); ma != nullptr) {

        const std::vector<VertexType>& vertices = ma->GetVertices();
        const std::vector<u32>& indices = ma->GetIndices();

        D3D11_BUFFER_DESC vertBufferDesc = {
            .ByteWidth = static_cast<UINT>(sizeof(VertexType) * vertices.size()),
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_VERTEX_BUFFER,
            .CPUAccessFlags = 0,
            .MiscFlags = 0,
            .StructureByteStride = sizeof(VertexType),
        };

        D3D11_SUBRESOURCE_DATA vertexBufferInitData = {
            .pSysMem = vertices.data(),
            // these have no meaning for vertex buffers
            .SysMemPitch = 0,
            .SysMemSlicePitch = 0,
        };

        if (auto res = m_device->CreateBuffer(&vertBufferDesc, &vertexBufferInitData, &m_vertexBuffer); FAILED(res)) {
            DXERROR(res);
        }

        D3D11_BUFFER_DESC indexBufferDesc = {
            .ByteWidth = static_cast<UINT>(sizeof(u32) * indices.size()),
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_INDEX_BUFFER,
            .CPUAccessFlags = 0,
            .MiscFlags = 0,
            .StructureByteStride = sizeof(u32),
        };

        D3D11_SUBRESOURCE_DATA indexBufferInitData = {
            .pSysMem = indices.data(),
            // these have no meaning for index buffers
            .SysMemPitch = 0,
            .SysMemSlicePitch = 0,
        };
        
        if (auto res = m_device->CreateBuffer(&indexBufferDesc, &indexBufferInitData, &m_indexBuffer); FAILED(res)) {
            DXERROR(res);
        }
    }
}


#pragma region debug print gltf file

void MeshAsset::GltfPrintInfo(cgltf_data* data) {
	for(int s = 0;s < data->scenes_count; ++s) {
		cgltf_scene scene = data->scenes[s];
		spdlog::info("[scene name={} nodes_count={}]", scene.name, scene.nodes_count);
	}
	
	GltfPrintMeshInfo(data);

	GltfPrintMaterialInfo(data);

	GltfPrintImageInfo(data);

	GltfPrintAnimationInfo(data);

	for(int a = 0; a < data->accessors_count; ++a) {
		cgltf_accessor accessor = data->accessors[a];
		spdlog::info("[accessor name={} comp_type={} is_sparse={}]", accessor.name, cgltf_component_type_strings[accessor.component_type], accessor.is_sparse);
	}
}


void MeshAsset::GltfPrintAnimationInfo(cgltf_data* data) {
	for(int a = 0; a < data->animations_count; ++a) {
		cgltf_animation animation = data->animations[a];
		spdlog::info("[animation name={}]", animation.name);

		for(int c = 0; c < animation.channels_count; ++c) {
			spdlog::info("[channel path_type={}]", cgltf_animation_path_type_strings[animation.channels[c].target_path]);
		}
	}
}

void MeshAsset::GltfPrintMaterialInfo(cgltf_data* data) {
	for(int m = 0;m < data->materials_count; ++m) {
		cgltf_material material = data->materials[m];

		spdlog::info("[material name={}]", material.name);
	}
}

void MeshAsset::GltfPrintImageInfo(cgltf_data* data) {
	for(int i = 0;i < data->images_count; ++i) {
		cgltf_image image = data->images[i];
		spdlog::info("[image name={} uri={} mime_type={}]", image.name, image.uri, image.mime_type);
	}	
}

void MeshAsset::GltfPrintMeshInfo(cgltf_data* data) {

	for(int m = 0;m < data->meshes_count; ++m) {
		cgltf_mesh mesh = data->meshes[m];
		spdlog::info("[mesh name={}]", mesh.name);
		
		for(int p = 0;p < mesh.primitives_count; ++p) {
			cgltf_primitive primitive = mesh.primitives[p];
			spdlog::info("[primitive type={}]", cgltf_primitive_type_strings[primitive.type]);

			for(int a = 0; a < primitive.attributes_count; ++a) {
				cgltf_attribute attribute = primitive.attributes[a];			
				spdlog::info("[attribute name={} index={} type={}]", attribute.name, attribute.index, cgltf_attribute_type_strings[attribute.type]);
			}

			spdlog::info("[indices name={} type={}]", primitive.indices->name, cgltf_component_type_strings[primitive.indices->component_type]);
		}

		for(int w = 0;w < mesh.weights_count; ++w) {
			cgltf_float weight = mesh.weights[w];
			spdlog::info("[weight value={}]", weight);
		}

		for(int tn = 0; tn < mesh.target_names_count; ++tn) {
			const char* tname = mesh.target_names[tn];
			spdlog::info("[target name={}]", tname);
		}

		for(int e = 0;e < mesh.extensions_count; ++e) {
			cgltf_extension extension = mesh.extensions[e];
			spdlog::info("[extension name={}]", extension.name);
		}
	}
}

#pragma endregion debug print gltf file