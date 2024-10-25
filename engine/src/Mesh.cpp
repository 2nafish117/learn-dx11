#include "Mesh.hpp"

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

	// data->meshes[7].primitives[6].attributes[4].name
	// do stuff

	GltfPrintInfo(data);

	cgltf_free(data);
}

MeshAsset::MeshAsset(const std::vector<Vertex>& vertices, const std::vector<u32>& indices) 
	: m_vertices(vertices), m_indices(indices)
{
	
}


void MeshAsset::GltfPrintInfo(cgltf_data* data) {

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
		}
	}
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