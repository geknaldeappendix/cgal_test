#define CGAL_DISABLE_ROUNDING_MATH_CHECK
#define CGAL_NO_ASSERTIONS
#define CGAL_NO_PRECONDITIONS
#define CGAL_NO_POSTCONDITIONS
#define CGAL_NO_WARNINGS

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Straight_skeleton_2.h>
#include <CGAL/create_straight_skeleton_2.h>
#include <CGAL/offset_polygon_2.h>
#include <CGAL/create_offset_polygons_2.h>

#include <vector>
#include <list>
#include <memory>

// Type definitions
typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef K::Point_2 Point_2;
typedef CGAL::Straight_skeleton_2<K> Ss;
typedef CGAL::Polygon_2<K> Polygon;
typedef std::shared_ptr<Ss> SsPtr;

// Enum to distinguish offset types
enum OffsetType {
    INTERIOR,
    EXTERIOR
};

// Wrapper class to manage skeleton and provide offset methods
class SkeletonManager {
private:
    Polygon original_polygon;
    SsPtr interior_skeleton;
    SsPtr exterior_skeleton;

    // Private method to compute skeleton if not already computed
    void compute_skeletons() {
        if (!interior_skeleton) {
            interior_skeleton = CGAL::create_interior_straight_skeleton_2(
                original_polygon.vertices_begin(), 
                original_polygon.vertices_end()
            );
        }
        
        if (!exterior_skeleton) {
            exterior_skeleton = CGAL::create_exterior_straight_skeleton_2(
                5,
                original_polygon.vertices_begin(), 
                original_polygon.vertices_end()
            );
        }

    }

public:
    // Constructor
    SkeletonManager(const Polygon& poly) : original_polygon(poly) {}

    // Compute offsets using precomputed skeletons
    emscripten::val offset_polygon(double offset_distance, int offset_type) {
        // Ensure skeletons are computed
        compute_skeletons();
        
        // Compute offset polygons
        std::vector<std::shared_ptr<Polygon>> offset_polygons;
        
        // Determine offset method based on type
        if (offset_type == OffsetType::INTERIOR) {
            offset_polygons = CGAL::create_offset_polygons_2<Polygon>(
                offset_distance, 
                *interior_skeleton
            );
        } else {
            offset_polygons = CGAL::create_offset_polygons_2<Polygon>(
                offset_distance, 
                *exterior_skeleton
            );
        }
        
        // Prepare return array
        emscripten::val result = emscripten::val::array();
        
        // Convert offset polygons
        for (const auto& offset_poly_ptr : offset_polygons) {
            emscripten::val js_polygon = emscripten::val::array();
            
            for (const auto& point : *offset_poly_ptr) {
                emscripten::val js_point = emscripten::val::object();
                js_point.set("x", CGAL::to_double(point.x()));
                js_point.set("y", CGAL::to_double(point.y()));
                js_polygon.call<void>("push", js_point);
            }
            
            result.call<void>("push", js_polygon);
        }
        
        return result;
    }

    // Expose straight skeleton information
    emscripten::val get_skeleton_info(int skeleton_type) {
        compute_skeletons();
        
        // Choose skeleton based on type
        SsPtr ss = (skeleton_type == OffsetType::INTERIOR) ? 
            interior_skeleton : exterior_skeleton;
        
        // Prepare return object
        emscripten::val result = emscripten::val::object();
        
        // Convert skeleton vertices
        emscripten::val vertices = emscripten::val::array();
        for (auto vi = ss->vertices_begin(); vi != ss->vertices_end(); ++vi) {
            Point_2 pt = vi->point();
            emscripten::val vertex = emscripten::val::object();
            vertex.set("x", CGAL::to_double(pt.x()));
            vertex.set("y", CGAL::to_double(pt.y()));
            vertices.call<void>("push", vertex);
        }
        result.set("vertices", vertices);
        
        // Convert skeleton half-edges
        emscripten::val edges = emscripten::val::array();
        for (auto ei = ss->halfedges_begin(); ei != ss->halfedges_end(); ++ei) {
            if (ei->is_border()) continue; // Skip border half-edges
            
            emscripten::val edge = emscripten::val::object();
            Point_2 source = ei->opposite()->vertex()->point();
            Point_2 target = ei->vertex()->point();
            
            emscripten::val start = emscripten::val::object();
            start.set("x", CGAL::to_double(source.x()));
            start.set("y", CGAL::to_double(source.y()));
            
            emscripten::val end = emscripten::val::object();
            end.set("x", CGAL::to_double(target.x()));
            end.set("y", CGAL::to_double(target.y()));
            
            edge.set("start", start);
            edge.set("end", end);
            
            edges.call<void>("push", edge);
        }
        result.set("edges", edges);
        
        return result;
    }

    // Wrapper function to create polygon from JavaScript array
    static SkeletonManager* create_from_js_array(const emscripten::val& points_array) {
        Polygon polygon;
        const int length = points_array["length"].as<int>();
        
        for (int i = 0; i < length; ++i) {
            emscripten::val point = points_array[i];
            double x = point["x"].as<double>();
            double y = point["y"].as<double>();
            polygon.push_back(Point_2(x, y));
        }
        
        return new SkeletonManager(polygon);
    }
};

// Bind functions to JavaScript
EMSCRIPTEN_BINDINGS(cgal_skeleton_module) {
    emscripten::enum_<OffsetType>("OffsetType")
        .value("INTERIOR", OffsetType::INTERIOR)
        .value("EXTERIOR", OffsetType::EXTERIOR);

    emscripten::class_<SkeletonManager>("SkeletonManager")
        .class_function("create", &SkeletonManager::create_from_js_array, emscripten::allow_raw_pointers())
        .function("offsetPolygon", &SkeletonManager::offset_polygon)
        .function("getSkeletonInfo", &SkeletonManager::get_skeleton_info);
}