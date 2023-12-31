#include <lxu_command.hpp>
#include <lx_layer.hpp>
#include <lx_mesh.hpp>
#include <lx_log.hpp>
#include <lx_io.hpp>
#include <lx_stddialog.hpp>

#include "xatlas.h"

#include <vector>
#include <array>
#include <mutex>

#include <iterator>
#include <algorithm>
#include <limits>
#include <stdint.h>

using namespace lx_err;

/* Print function to write XA_PRINT to log */
static int PrintToLog(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	// Format the message
	std::string message(254, '\0');
	int result = std::vsnprintf(&(*message.begin()), 255, format, args);

	CLxUser_LogService log_service;
	CLxUser_Log log;
	if(log_service.GetSubSystem(LXsLOG_LOGSYS, log))
	{
		CLxUser_LogEntry entry;
		if(log_service.NewEntry(LXe_INFO, message.c_str(), entry))
			log.AddEntry(entry);
	}
	va_end(args);
	return result;
}

struct PolyVertex {
	LXtPolygonID poly;
	LXtPointID point;
	PolyVertex(LXtPolygonID p, LXtPointID v) {poly = p; point = v;}
};

/* For each layer, save a vector for poly vertex lookup, */
struct Layer {
	std::vector<PolyVertex> poly_vertices;
};

class UVMapVisitor : public CLxImpl_AbstractVisitor
{
	CLxUser_MeshMap* vmap;
	std::set<std::string> names;

	virtual LxResult Evaluate()
	{
		const char* mapName;
		if (LXx_OK(vmap->Name(&mapName))) {
			names.insert(std::string(mapName));
		}
		return LXe_OK;
	}

public:
	UVMapVisitor(CLxUser_MeshMap* vmap) { this->vmap = vmap; }
	std::set<std::string> GetNames() { return names; }
	void ClearNames() { names.clear(); }
};

class XAtlasCommand : public CLxBasicCommand
{
public:
	XAtlasCommand();

	int basic_CmdFlags() LXx_OVERRIDE;
	void basic_Execute(unsigned int flags);

	LxResult cmd_DialogInit() LXx_OVERRIDE;
	LxResult cmd_DialogFormatting(const char** formatting) LXx_OVERRIDE;
	LxResult cmd_Query(unsigned int index, ILxUnknownID value_array) LXx_OVERRIDE;

	LxResult atrui_UIHints(unsigned int index, ILxUnknownID hints) LXx_OVERRIDE;
};

XAtlasCommand::XAtlasCommand() 
{
	// Chart Options, see xatlas.h ChartOptions
	dyna_Add("maxChartArea", LXsTYPE_FLOAT);
	dyna_Add("maxBoundaryLength", LXsTYPE_FLOAT);

	dyna_Add("normalDeviationWeight", LXsTYPE_FLOAT);
	dyna_Add("roundnessWeight", LXsTYPE_FLOAT);
	dyna_Add("straightnessWeight", LXsTYPE_FLOAT);
	dyna_Add("normalSeamWeight", LXsTYPE_FLOAT);
	dyna_Add("textureSeamWeight", LXsTYPE_FLOAT);

	dyna_Add("maxCost", LXsTYPE_FLOAT);
	dyna_Add("maxIteration", LXsTYPE_INTEGER);

	dyna_Add("useInputMeshUvs", LXsTYPE_BOOLEAN);
	dyna_Add("fixWinding", LXsTYPE_BOOLEAN); // 10
	
	// Pack Options, see xatlas.h PackOptions
	dyna_Add("maxChartSize", LXsTYPE_INTEGER);

	dyna_Add("padding", LXsTYPE_INTEGER);

	dyna_Add("texelsPerUnit", LXsTYPE_FLOAT);

	dyna_Add("resolution", LXsTYPE_INTEGER); // 14

	dyna_Add("bilinear", LXsTYPE_BOOLEAN);
	dyna_Add("blockAlign", LXsTYPE_BOOLEAN);
	dyna_Add("bruteForce", LXsTYPE_BOOLEAN);
	dyna_Add("createImage", LXsTYPE_BOOLEAN); // 18
	dyna_Add("rotateChartsToAxis", LXsTYPE_BOOLEAN);
	dyna_Add("rotateCharts", LXsTYPE_BOOLEAN);

	dyna_Add("vmap", LXsTYPE_VERTMAPNAME);
	dyna_SetFlags(21, LXfCMDARG_QUERY);
}

/* Set the default values to all arguments, if they were not set by user. */
LxResult XAtlasCommand::cmd_DialogInit()
{
	if (!dyna_IsSet(0))
		attr_SetFlt(0, 0.0);
	if (!dyna_IsSet(1))
		attr_SetFlt(1, 0.0);

	if (!dyna_IsSet(2))
		attr_SetFlt(2, 2.0);
	if (!dyna_IsSet(3))
		attr_SetFlt(3, 0.01);
	if (!dyna_IsSet(4))
		attr_SetFlt(4, 6.0);
	if (!dyna_IsSet(5))
		attr_SetFlt(5, 4.0);
	if (!dyna_IsSet(6))
		attr_SetFlt(6, 0.5);

	if (!dyna_IsSet(7))
		attr_SetFlt(7, 2.0);
	if (!dyna_IsSet(8))
		attr_SetInt(8, 1);

	if (!dyna_IsSet(9))
		attr_SetBool(9, 0);
	if (!dyna_IsSet(10))
		attr_SetBool(10, 0);

	if (!dyna_IsSet(11))
		attr_SetInt(11, 0);
	if (!dyna_IsSet(12))
		attr_SetInt(12, 0);

	if (!dyna_IsSet(13))
		attr_SetFlt(13, 0.0);

	if (!dyna_IsSet(14))
		attr_SetInt(14, 0);

	if (!dyna_IsSet(15))
		attr_SetBool(15, 0);

	if (!dyna_IsSet(16))
		attr_SetBool(16, 0);

	if (!dyna_IsSet(17))
		attr_SetBool(17, 0);

	// Do not create the image by default, we don't have any current way of exposing it to users either way.
	if (!dyna_IsSet(18))
		attr_SetBool(18, 0);

	if (!dyna_IsSet(19))
		attr_SetBool(19, 1);

	if (!dyna_IsSet(20))
		attr_SetBool(20, 1);

	return LXe_OK;
}

/* Format the dialog, adding named separators */
LxResult XAtlasCommand::cmd_DialogFormatting(const char** formatting)
{
	*formatting = "-[@xatlas@chartOptions@] 0 1 2 3 4 5 6 7 8 9 10 -[@xatlas@packOptions@] 11 12 13 14 15 16 17 19 20 - 21";
	return LXe_OK;
}

/* Set additional UI settings, like min allowed value etc */
LxResult XAtlasCommand::atrui_UIHints(unsigned int index, ILxUnknownID hints)
{
	CLxUser_UIHints ui_hints(hints);
	switch (index)
	{
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 13:
			ui_hints.MinFloat(0.0);
			break;
		case 8: // maxIterations
			ui_hints.MinInt(1);
			break;
		case 11:
		case 12:
		case 14:
			ui_hints.MinInt(0);
			break;	
		case 21: // vmap
			ui_hints.VertmapAllowNone(false);
			ui_hints.VertmapItemMode(true);
			break;
	}
	return LXe_OK;
}

int XAtlasCommand::basic_CmdFlags()
{
	return LXfCMD_MODEL | LXfCMD_UNDO;
}

LxResult XAtlasCommand::cmd_Query(unsigned int index, ILxUnknownID value_array)
{
	CLxUser_ValueArray va(value_array);
	if (index == 21) {
		CLxUser_LayerService layer_service;
		CLxUser_LayerScan layer_scan;
		CLxUser_Mesh mesh;
		CLxUser_MeshMap vmap;
		std::set<std::string> name_set;

		check(layer_service.ScanAllocate(LXf_LAYERSCAN_ACTIVE, layer_scan));
		unsigned layer_count;
		layer_scan.Count(&layer_count);
		for (unsigned layer_index = 0; layer_index < layer_count; layer_index++)
		{
			layer_scan.MeshBase(layer_index, mesh);
			mesh.GetMaps(vmap);
			UVMapVisitor visitor(&vmap);
			vmap.FilterByType(LXi_VMAP_TEXTUREUV);
			vmap.Enum(&visitor);

			std::set<std::string> names = visitor.GetNames();
			name_set.insert(names.begin(), names.end());
		}
		layer_scan.Apply();
		layer_scan.clear();
		layer_scan = NULL;

		if (name_set.empty())
			return LXe_FAILED;
		else {
			auto name = name_set.begin();
			va.AddString(name->c_str());
		}
	}

	return LXe_OK;
}

void XAtlasCommand::basic_Execute(unsigned int flags) 
{
	std::string vmap_name;
	if (dyna_IsSet(21))
		dyna_String(21, vmap_name);

	// If no texture vmap was selected in the dropdown, fail and tell the users why
	if (vmap_name.empty())
	{
		basic_Message().SetMsg("No texture selected.");
		throw(LXe_FAILED);
	}

	std::vector<Layer> layers;

	CLxUser_LayerService layer_service;
	CLxUser_LayerScan layer_scan;

	// lx mesh component accessors,
	CLxUser_Item item;
	CLxUser_Mesh mesh;
	CLxUser_Point vert;
	CLxUser_Polygon poly;
	CLxUser_MeshMap vmap;

	check(layer_service.ScanAllocate(LXf_LAYERSCAN_ACTIVE | LXf_LAYERSCAN_MARKPOLYS, layer_scan));
	
	uint32_t layer_count;
	check(layer_scan.Count(&layer_count));

	if (layer_count == 0)
	{
		layer_scan.Apply();
		layer_scan.clear();
		basic_Message().SetMsg("No active layers.");
		throw(LXe_FAILED);
	}

	CLxUser_MeshService mesh_service;
	LXtMarkMode selected;
	mesh_service.ModeCompose(LXsMARK_SELECT, nullptr, &selected);
	LXtMarkMode hidden;
	mesh_service.ModeCompose(LXsMARK_HIDE, nullptr, &hidden);

	LXtFVector pos;
	LXtFVector2 uv;
	LXtVector normal;

	LXtPointID vert_id;
	LXtPolygonID poly_id;
	LXtMeshMapID vmap_id;

	// Get material names from calling 'query layerservice material ...'
	std::vector<std::string> material_names;
	CLxUser_CommandService command_service;
	CLxUser_Command spawned_command;
	CLxUser_ValueArray va;
	command_service.Spawn(LXiCTAG_NULL, "query", spawned_command);

	command_service.QueryArgString(spawned_command, 0, "layerservice material.N ?", va, 0, false);

	int material_count;
	va.GetInt(0, &material_count);

	for (int i = 0; i < material_count; i++)
	{
		std::string material_name;
		std::string argument = "layerservice material.name ? " + std::to_string(i);
		command_service.QueryArgString(spawned_command, 0, argument.c_str(), va, 0, false);
		va.GetString(0, material_name);
		material_names.push_back(material_name);
	}

	xatlas::SetPrint(PrintToLog, true);
	xatlas::Atlas* atlas = xatlas::Create();	

	// Pass arguments to the option structs
	xatlas::ChartOptions chartOptions;
	chartOptions.maxChartArea = dyna_Float(0, 0.0f);
	chartOptions.maxBoundaryLength = dyna_Float(1, 0.0f);
	chartOptions.normalDeviationWeight = dyna_Float(2, 2.0f);
	chartOptions.roundnessWeight = dyna_Float(3, 0.01f);
	chartOptions.straightnessWeight = dyna_Float(4, 6.0f);
	chartOptions.normalSeamWeight = dyna_Float(5, 4.0f);
	chartOptions.textureSeamWeight = dyna_Float(6, 0.5f);
	chartOptions.maxCost = dyna_Float(7, 2.0f);
	chartOptions.maxIterations = dyna_Int(8, 1);
	chartOptions.useInputMeshUvs = dyna_Bool(9, false);
	chartOptions.fixWinding = dyna_Bool(10, false);

	xatlas::PackOptions packOptions;
	packOptions.maxChartSize = dyna_Int(11, 0);
	packOptions.padding = dyna_Int(12, 0);
	packOptions.texelsPerUnit = dyna_Float(13, 0.0f);
	packOptions.resolution = dyna_Int(14, 0);
	packOptions.bilinear = dyna_Bool(15, true);
	packOptions.blockAlign = dyna_Bool(16, true);
	packOptions.bruteForce = dyna_Bool(17, true);
	packOptions.createImage = dyna_Bool(18, true);
	packOptions.rotateChartsToAxis = dyna_Bool(19, true);
	packOptions.rotateCharts = dyna_Bool(20, true);

	// Add meshes to atlas,
	for (uint32_t layer_index = 0; layer_index < layer_count; layer_index++)
	{
		std::vector<unsigned int> indices;
		std::vector<uint32_t> face_material_data;
		std::vector<float> positions;
		std::vector<float> texcoords;
		std::vector<float> normals;
		std::vector<uint8_t> face_ignore_data;

		Layer layer;

		layer_scan.BaseMeshByIndex(layer_index, mesh);

		// Get 'Accessors' for Modo SDK mesh components,
		vert.fromMesh(mesh);
		poly.fromMesh(mesh);
		vmap.fromMesh(mesh);

		vmap.SelectByName(LXi_VMAP_TEXTUREUV, vmap_name.c_str());

		// In the case users have multiple layers selected, and picked a vmap only available for subset
		// the vmap will be invalid for some layers.
		// Not sure if users then expect that vmap to be created for current layer...
		if (!vmap.test()) 
		{
			xatlas::Destroy(atlas);
			const char* item_name;
			item.Name(&item_name);
			std::string error_message = vmap_name + " is not valid for " + item_name;
			basic_Message().SetMsg(error_message.c_str());
			throw(LXe_FAILED);
		}

		vmap_id = vmap.ID();

		// For each polygon, get uv & normal of poly-vert and push to texcoords and indices
		uint32_t polygon_count = 0;
		uint32_t total_triangle_count = 0;
		mesh.PolygonCount(&polygon_count);
		for (uint32_t poly_index = 0; poly_index < polygon_count; poly_index++)
		{
			poly.SelectByIndex(poly_index);
			poly_id = poly.ID();

			// Find material index for polygon,
			CLxUser_StringTag ptag(poly);
			uint32_t ptag_count;
			ptag.Count(&ptag_count);
			int material_index = -1;
			for (uint32_t ptag_index = 0; ptag_index < ptag_count; ptag_index++)
			{
				const char* ptag_tag;
				LXtID4 ptag_type;
				ptag.ByIndex(ptag_index, &ptag_type, &ptag_tag);
				if (ptag_type == LXxID4('M', 'A', 'T', 'R'))
				{
					for (int i = 0; i < material_names.size(); i++)
					{
						if (ptag_tag == material_names[i])
						{
							material_index = i;
						}
					}
					break;
				}
			}

			// Don't atlas faces set to true, so we won't atlas hidden or unselected faces.
			bool ignore = false;
			CLxResult polygon_hidden = poly.TestMarks(hidden);
			CLxResult polygon_selected = poly.TestMarks(selected);
			if (polygon_hidden.isTrue() || polygon_selected.isFalse())
			{
				ignore = true;
			}

			// For each triangle,
			uint32_t triangle_count;
			poly.GenerateTriangles(&triangle_count);
			for (uint32_t triangle_index = 0; triangle_index < triangle_count; triangle_index++)
			{
				LXtPointID triangle_points[3];
				poly.TriangleByIndex(triangle_index, triangle_points, triangle_points+1, triangle_points+2);

				face_material_data.push_back(material_index);
				face_ignore_data.push_back(ignore);

				// For each point in the triangle,
				for (int i = 0; i < 3; i++)
				{
					vert.Select(triangle_points[i]);

					vert.Pos(pos);
					positions.push_back(pos[0]);
					positions.push_back(pos[1]);
					positions.push_back(pos[2]);

					// passing uv values to xatlas is optional, only used as hint
					LxResult map_evaluate = poly.MapEvaluate(vmap_id, triangle_points[i], uv);
					if (map_evaluate == LXe_OK)
					{
						texcoords.push_back(uv[0]);
						texcoords.push_back(uv[1]);
					}
					else if (chartOptions.useInputMeshUvs)
					{
						// If we failed to get uvs and want to use input mesh uvs, tell users they need to map uvs
						xatlas::Destroy(atlas);
						basic_Message().SetMsg("Unmapped UVs");
						throw(LXe_FAILED);
					}
					else
					{
						texcoords.push_back(0.0f);
						texcoords.push_back(0.0f);
					}

					vert.Normal(poly_id, normal);
					normals.push_back(normal[0]);
					normals.push_back(normal[1]);
					normals.push_back(normal[2]);

					layer.poly_vertices.push_back(PolyVertex(poly_id, triangle_points[i]));
					indices.push_back(total_triangle_count * 3 + i);
				}
				total_triangle_count++;
			}
		}

		layers.push_back(layer);

		xatlas::MeshDecl mesh_decl;

		mesh_decl.vertexCount = positions.size() / 3;

		mesh_decl.vertexPositionData = positions.data();
		mesh_decl.vertexPositionStride = sizeof(float) * 3;

		mesh_decl.vertexUvData = texcoords.data();
		mesh_decl.vertexUvStride = sizeof(float) * 2;

		mesh_decl.vertexNormalData = normals.data();
		mesh_decl.vertexNormalStride = sizeof(float) * 3;

		mesh_decl.indexCount = (uint32_t)indices.size();
		mesh_decl.indexData = indices.data();
		mesh_decl.indexFormat = xatlas::IndexFormat::UInt32;

		mesh_decl.faceMaterialData = face_material_data.data();		
		mesh_decl.faceIgnoreData = (const bool*)face_ignore_data.data();

		xatlas::AddMeshError error = xatlas::AddMesh(atlas, mesh_decl, layer_count);

		// These errors are more likely for developers, not end users
		if (error != xatlas::AddMeshError::Success) {
			xatlas::Destroy(atlas);
			basic_Message().SetMsg(xatlas::StringForEnum(error));
			throw(LXe_FAILED);
		}
	}

	layer_scan.Apply();
	layer_scan.clear();
	layer_scan = NULL;

	// TODO: figure out how to pass lx.monitor to the progress callback
	xatlas::ComputeCharts(atlas, chartOptions);
	xatlas::PackCharts(atlas, packOptions);

	// Check that the packing went fine, if atlas has height or width of zero, it failed
	if (atlas->width == 0 || atlas->height == 0) {
		xatlas::Destroy(atlas);
		basic_Message().SetMsg("Unknown issue, please provide scene and reproduction steps");
		throw(LXe_FAILED);
	}

	CLxUser_LayerScan layer_scan_edit;
	layer_service.ScanAllocate(LXf_LAYERSCAN_EDIT, layer_scan_edit);
	layer_scan_edit.Count(&layer_count);

	if (atlas->meshCount != layer_count)
	{
		xatlas::Destroy(atlas);
		basic_Message().SetMsg("Mesh count mismatch");
		throw(LXe_FAILED);
	}

	// Assuming layers are accessed in same order, and that atlas stores them in order it was given meshes.
	// iterate over all layers, and get the xatlas mesh for that layer, then loop over all vertices and 
	// set the value to the lx mesh.
	for (uint32_t layer_index = 0; layer_index < layer_count; layer_index++)
	{
		xatlas::Mesh &xmesh = atlas->meshes[layer_index];

		layer_scan_edit.MeshEdit(layer_index, mesh);

		vert.fromMesh(mesh);
		poly.fromMesh(mesh);
		vmap.fromMesh(mesh);

		vmap.SelectByName(LXi_VMAP_TEXTUREUV, vmap_name.c_str());
		vmap_id = vmap.ID();

		mesh.BeginEditBatch();

		// For each vertex in output mesh, get the xref and uv to set the
		for (uint32_t i = 0; i < xmesh.vertexCount; i++)
		{
			xatlas::Vertex &v = xmesh.vertexArray[i];

			// Skip if vertex is not present in any atlas or chart.
			if (v.atlasIndex < 0 || v.chartIndex < 0)
				continue;

			// Normalize the computed uv coordinates from xatlas, output uvs are in atlas range
			float maxSize = atlas->width <= atlas->height ? (float)atlas->height : (float)atlas->width;
			uv[0] = v.uv[0] / maxSize;
			uv[1] = v.uv[1] / maxSize;

			// xatlas::Vertex.xref will give us the index of the original indices array,
			PolyVertex pv = layers[layer_index].poly_vertices[v.xref];

			// Set the vmap value for the poly vertex,
			poly.Select(pv.poly);
			poly.SetMapValue(pv.point, vmap_id, uv);
		}

		mesh.EndEditBatch();

		mesh.SetMeshEdits(LXf_MESHEDIT_MAP_UV);
		layer_scan_edit.SetMeshChange(layer_index, LXf_MESHEDIT_MAP_UV);
		layer_scan_edit.Update();
	}

	layer_scan_edit.Apply();
	layer_scan_edit.clear();
	layer_scan_edit = NULL;

	xatlas::Destroy(atlas);
}

// Entry point for the plug-in
void initialize()
{
	CLxGenericPolymorph *server;
	
	server = new CLxPolymorph<XAtlasCommand>;

	server->AddInterface(new CLxIfc_Command<XAtlasCommand>);
	server->AddInterface(new CLxIfc_Attributes<XAtlasCommand>);
	server->AddInterface(new CLxIfc_AttributesUI<XAtlasCommand>);

	lx::AddServer("xatlas", server);
}