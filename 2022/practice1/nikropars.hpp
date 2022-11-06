#pragma once

#include <vector>
#include <array>
#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <map>

class scene {
	template <std::size_t size>
	static void push_attrib(std::istringstream &parser, std::vector<std::array<float, size>> &attribs) {
		attribs.push_back({});
		auto &new_memb = *attribs.rbegin();
		for (auto i = 0; i < size; ++i)
			parser >> new_memb[i];
	}

	void read_mtllib(std::filesystem::path &path) {
		std::ifstream in(path);

		std::string line;
		std::size_t count = 0;

		std::string name;
		
		material mtl;

		while (std::getline(in >> std::ws, line)) {
			++count;

			if (line.empty() || line[0] == '#')
				continue;

			std::istringstream parser(std::move(line));

			std::string tag;
			parser >> tag;

			if (tag == "newmtl") {
				if (name != "")
					material_map[name] = mtl;
				parser >> name;
			}
			else if (tag == "Ks") {
				auto &gl = mtl.glossiness;
				parser >> gl[0] >> gl[1] >> gl[2];
			}
			else if (tag == "Ns") {
				parser >> mtl.roughness;
			}
			else if (tag == "map_Ka") {
				std::string texture_name;
				parser >> texture_name;
				mtl.albedo = path.parent_path();
				mtl.albedo += "/" + texture_name;
			}
			else if (tag == "map_d") {
				std::string texture_name;
				parser >> texture_name;
				mtl.transparency = path.parent_path();
				mtl.transparency += "/" + texture_name;
			}
		}

		material_map[name] = mtl;
	}

public:
	class material {
	public:
		std::array<float, 3> glossiness;
		float roughness;
		std::filesystem::path albedo;
		std::filesystem::path transparency;
	};

	std::map<std::string, material> material_map;

	class group {
	public:
		material mtl;
		std::vector<std::uint32_t> indices;
	};

	class vertex {
	public:
		std::array<float, 3> position;
		std::array<float, 3> normal;
		std::array<float, 2> texture_coord;
	};

	std::vector<vertex> verticies;
	std::map<std::string, group> objects;

	scene(const std::filesystem::path &path) {
		std::ifstream in(path);

		std::map<std::array<std::uint32_t, 3>, std::uint32_t> finder;

		std::string cur_mtl_name;
		std::string cur_group_name;
		group cur_group;

		std::string line;
		std::size_t count = 0;

		std::vector<std::array<float, 3>> positions;
		std::vector<std::array<float, 3>> normals;
		std::vector<std::array<float, 2>> texture_coords;

		while (std::getline(in >> std::ws, line)) {
			++count;

			if (line.empty() || line[0] == '#')
				continue;

			std::istringstream parser(std::move(line));

			std::string tag;
			parser >> tag;

			if (tag == "mtllib") {
				std::string name;
				parser >> name;
				std::filesystem::path mtlpath = path.parent_path();
				mtlpath += "/" + name;
				read_mtllib(mtlpath);
			}
			else if (tag == "usemtl") {
				parser >> cur_mtl_name;
				cur_group.mtl = material_map[cur_mtl_name];
			}
			else if (tag == "g") {
				if (!cur_group_name.empty())
					objects[cur_group_name] = cur_group;
				parser >> cur_group_name;
			}
			else if (tag == "v")
				push_attrib(parser, positions);
			else if (tag == "vn")
				push_attrib(parser, normals);
			else if (tag == "vt")
				push_attrib(parser, texture_coords);
			else if (tag == "f") {
				std::vector<std::uint32_t> vert;

				while (parser) {
					std::array<std::int32_t, 3> idx({0, 0, 0});

					parser >> idx[0];

					if (parser.eof())
						break;

					if (!parser)
						throw std::exception("not enough indices");

					if (!std::isspace(parser.peek()) && !parser.eof()) {
						if (parser.get() != '/')
							throw std::exception("expected symbol '/'");

						if (parser.peek() != '/') { // continue with texture and normal
							parser >> idx[1];
							if (!parser)
								throw std::exception("expected texture coord index");

							if (!std::isspace(parser.peek()) && !parser.eof()) {
								if (parser.get() != '/')
									throw std::exception("expected symbol '/'");

								parser >> idx[2];
								if (!parser)
									throw std::exception("expected normal index");
							}
						}
						else {
							parser.get();

							parser >> idx[2];
							if (!parser)
								throw std::exception("expected normal index");
						}
					}

					if (abs(idx[0]) > positions.size())
						throw std::exception("bad attrib indices");
					if (abs(idx[1]) > texture_coords.size())
						throw std::exception("bad attrib indices");
					if (abs(idx[2]) > normals.size())
						throw std::exception("bad attrib indices");

					std::array<std::uint32_t, 3> true_index;


					if (idx[0] < 0)
						true_index[0] = positions.size() + idx[0];
					else
						true_index[0] = idx[0] - 1;

					if (idx[1] < 0)
						true_index[1] = texture_coords.size() + idx[1];
					else
						true_index[1] = idx[1] - 1;
					
					if (idx[2] < 0)
						true_index[2] = normals.size() + idx[2];
					else
						true_index[2] = idx[2] - 1;

					if (finder.find(true_index) == finder.end()) {
						finder[true_index] = verticies.size();
						verticies.push_back({});
						vertex &v = *verticies.rbegin();
						v.position = positions[true_index[0]];
						if (idx[1] != 0)
							v.texture_coord = texture_coords[true_index[1]];
						else
							v.texture_coord = { 0, 0 };

						if (idx[2] != 0)
							v.normal = normals[true_index[2]];
						else
							v.normal = { 0, 0, 0 };
					}

					vert.push_back(finder[true_index]);
				}

				for (std::size_t i = 1; i < vert.size() - 1; ++i) {
					cur_group.indices.push_back(vert[0]);
					cur_group.indices.push_back(vert[i]);
					cur_group.indices.push_back(vert[i + 1]);
				}
			}
		}

		objects[cur_group_name] = cur_group;
	}
};
