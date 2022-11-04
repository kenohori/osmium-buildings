#include <iostream>
#include <fstream>

#include <osmium/area/assembler.hpp>
#include <osmium/area/multipolygon_manager.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/io/pbf_input.hpp>
#include <osmium/visitor.hpp>
#include <osmium/index/map/flex_mem.hpp>

#include <h3/h3api.h>

using index_type = osmium::index::map::FlexMem<osmium::unsigned_object_id_type, osmium::Location>;
using location_handler_type = osmium::handler::NodeLocationsForWays<index_type>;

class BuildingHandler : public osmium::handler::Handler {
  LatLng ll;
  H3Index i;
public:
  const int h3_resolution = 4;
  std::size_t buildings;
  std::unordered_map<H3Index, std::size_t> buildings_total;
  
  BuildingHandler() {
    buildings = 0;
  }
  
  void area(const osmium::Area& area) {
    double sum_lon = 0, sum_lat = 0;
    std::size_t num_points = 0;
    for (auto const &ring: area.outer_rings()) {
      for (auto const &node: ring) {
        sum_lon += node.lon();
        sum_lat += node.lat();
        ++num_points;
      }
    } if (num_points > 0) {
      ll.lng = M_PI*sum_lon/(num_points*180.0);
      ll.lat = M_PI*sum_lat/(num_points*180.0);
      if (buildings % 1000000 == 0) std::cout << "\t" << buildings/1000000 << "m buildings processed " << std::endl;
      ++buildings;
      latLngToCell(&ll, h3_resolution, &i);
      ++buildings_total[i];
    }
  }
};

int main(int argc, const char * argv[]) {
  
  BuildingHandler handler;
  
  std::vector<std::string> input_paths;
  const std::filesystem::path data_folder{"/Users/ken/Downloads/osm/"}; // folder with OSM .pbf files
  for (auto const& input_path : std::filesystem::directory_iterator{data_folder}) {
    if (input_path.path().extension() == ".pbf") {
      input_paths.push_back(input_path.path().string());
    } else {
      std::cout << "Skipping " << input_path.path().string() << "..." << std::endl;
    }
  }

  for (auto const &input_path: input_paths) {
    std::cout << "Reading input: " << input_path << "..." << std::endl;
    
    osmium::io::File input_file{input_path};
    osmium::area::Assembler::config_type assembler_config;
    
    osmium::TagsFilter filter{false};
    filter.add_rule(true, "building");
    
    osmium::area::MultipolygonManager<osmium::area::Assembler> mp_manager{assembler_config, filter};
    
    // First pass
    std::cout << "\tReading relations...";
    osmium::relations::read_relations(input_file, mp_manager);
    std::cout << " done" << std::endl;
    
    index_type index;
    location_handler_type location_handler{index};
    location_handler.ignore_errors();
    
    std::cout << "\tReading objects..." << std::endl;
    osmium::io::Reader reader{input_file};
    osmium::apply(reader, location_handler, mp_manager.handler([&handler](osmium::memory::Buffer&& buffer) {
      osmium::apply(buffer, handler);
    }));
    reader.close();
  }
  
  std::ofstream output_stream;
  output_stream.open("/Users/ken/Versioned/my-website/maps/osm-buildings/buildings.csv"); // output csv file
  output_stream << "lat,lng,buildings\n";
  
  for (auto const &cell: handler.buildings_total) {
    LatLng ll;
    cellToLatLng(cell.first, &ll);
    output_stream << 180.0*ll.lat/M_PI << "," << 180.0*ll.lng/M_PI << "," << cell.second << "\n";
  }
  
  output_stream.close();
  
  return 0;
}
