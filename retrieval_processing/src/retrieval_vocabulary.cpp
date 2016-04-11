#include <object_3d_retrieval/supervoxel_segmentation.h>
#include <object_3d_retrieval/pfhrgb_estimation.h>
#include <dynamic_object_retrieval/visualize.h>
#include <dynamic_object_retrieval/surfel_type.h>
#include <dynamic_object_retrieval/summary_types.h>
#include <dynamic_object_retrieval/summary_iterators.h>
#include "dynamic_object_retrieval/extract_surfel_features.h"

#include <pcl/io/pcd_io.h>
#include <pcl/filters/approximate_voxel_grid.h>

#include <metaroom_xml_parser/load_utilities.h>
#include <dynamic_object_retrieval/definitions.h>

#include <ros/ros.h>
#include <std_msgs/String.h>

using namespace std;

POINT_CLOUD_REGISTER_POINT_STRUCT (HistT,
                                   (float[N], histogram, histogram)
)

using PointT = pcl::PointXYZRGB;
using CloudT = pcl::PointCloud<PointT>;
using NormalT = pcl::Normal;
using NormalCloudT = pcl::PointCloud<NormalT>;
using Graph = supervoxel_segmentation::Graph;
using HistT = pcl::Histogram<N>;
using HistCloudT = pcl::PointCloud<HistT>;
using SurfelT = SurfelType;
using SurfelCloudT = pcl::PointCloud<SurfelT>;
using VocT = grouped_vocabulary_tree<HistT, 8>;
using IndexT = VocT::index_type;
using AdjacencyT = vector<set<pair<int, int> > >;

// we should retain the vocabulary here if trained
// and check at startup if it is available,
// does that mean we need to provide the querying
// interface as well? maybe
ros::Publisher pub;
int min_training_sweeps;
size_t nbr_added_sweeps;
boost::filesystem::path vocabulary_path;
VocT* vt;

set<pair<int, int> > compute_group_adjacencies(const boost::filesystem::path& segment_path)
{
    set<pair<int, int> > adjacencies;

    boost::filesystem::path graph_path = segment_path / "graph.cereal";
    supervoxel_segmentation ss;
    supervoxel_segmentation::Graph g;
    cout << "Loading graph " << graph_path.string() << endl;
    ss.load_graph(g, graph_path.string());

    typename boost::property_map<supervoxel_segmentation::Graph, boost::vertex_name_t>::type vertex_name = boost::get(boost::vertex_name, g);

    // now iterate over all of the adjacencies in the original graph and add them to the set data structure
    using edge_iterator = boost::graph_traits<supervoxel_segmentation::Graph>::edge_iterator;
    edge_iterator edge_it, edge_end;
    for (tie(edge_it, edge_end) = boost::edges(g); edge_it != edge_end; ++edge_it) {
        supervoxel_segmentation::Vertex u = source(*edge_it, g);
        supervoxel_segmentation::Vertex v = target(*edge_it, g);
        supervoxel_segmentation::vertex_name_property from = boost::get(vertex_name, u);
        supervoxel_segmentation::vertex_name_property to = boost::get(vertex_name, v);
        adjacencies.insert(make_pair(from.m_value, to.m_value));
    }

    return adjacencies;
}

/*
void add_to_vocabulary(HistCloudT::Ptr& features, vector<IndexT>& indices, AdjacencyT& adjacencies)
{

}
*/

// we need to split this into 2 parts if we are to keep the same structure
// as before, one for training and one for adding?
void train_vocabulary(const boost::filesystem::path& data_path)
{
    dynamic_object_retrieval::convex_feature_cloud_map segment_features(data_path);
    dynamic_object_retrieval::convex_keypoint_cloud_map segment_keypoints(data_path);
    dynamic_object_retrieval::convex_sweep_index_map sweep_indices(data_path);
    dynamic_object_retrieval::convex_segment_map segment_paths(data_path);
    dynamic_object_retrieval::islast_convex_segment_map last_segments(data_path);

    cout << __FILE__ << ", " << __LINE__ << endl;

    // this is really the last thing we need to add.

    dynamic_object_retrieval::vocabulary_summary summary;
    //summary.load(vocabulary_path);
    summary.min_segment_features = 30;
    summary.max_training_features = 400000; // 150000;
    summary.max_append_features = 1000000; // 1000000;
    summary.vocabulary_type = "incremental";
    summary.subsegment_type = "convex_segment";
    summary.noise_data_path = data_path.string();
    summary.annotated_data_path = data_path.string();

    cout << __FILE__ << ", " << __LINE__ << endl;

    size_t min_segment_features = summary.min_segment_features;
    size_t max_training_features = summary.max_training_features;
    size_t max_append_features = summary.max_append_features;

    cout << __FILE__ << ", " << __LINE__ << endl;

    vt = new VocT(vocabulary_path.string());
    vt->set_min_match_depth(3);

    cout << __FILE__ << ", " << __LINE__ << endl;

    HistCloudT::Ptr features(new HistCloudT);
    //CloudT::Ptr centroids(new CloudT);
    AdjacencyT adjacencies;
    vector<IndexT> indices;

    cout << __FILE__ << ", " << __LINE__ << endl;

    size_t counter = 0; // index among all segments
    size_t sweep_i; // index of sweep
    size_t last_sweep = 0; // last index of sweep
    size_t sweep_counter = 0; // index within sweep
    bool training = true;
    boost::filesystem::path segment_path;
    // add an iterator with the segment nbr??? maybe not
    // but! add an iterator with the sweep nbr!
    for (auto tup : dynamic_object_retrieval::zip(segment_features, segment_keypoints, sweep_indices, segment_paths, last_segments)) {

        cout << __FILE__ << ", " << __LINE__ << endl;

        HistCloudT::Ptr features_i;
        CloudT::Ptr keypoints_i;
        boost::filesystem::path last_segment = segment_path;
        bool islast;
        tie(features_i, keypoints_i, sweep_i, segment_path, islast) = tup;

        cout << __FILE__ << ", " << __LINE__ << endl;

        //cout << "Sweep: " << sweep_i << endl;

        // train on a subset of the provided features
        if (sweep_i != last_sweep) {
            adjacencies.push_back(compute_group_adjacencies(last_segment.parent_path()));

            cout << __FILE__ << ", " << __LINE__ << endl;

            if (training && last_sweep >= min_training_sweeps - 1) {// features->size() > max_training_features) {
                vt->set_input_cloud(features, indices);
                vt->add_points_from_input_cloud(adjacencies, false);
                features->clear();
                indices.clear();
                adjacencies.clear();
                training = false;
            }

            cout << __FILE__ << ", " << __LINE__ << endl;

            if (!training && features->size() > max_append_features) {
                cout << "Appending " << features->size() << " points in " << adjacencies.size() << " groups" << endl;
                cout << adjacencies.size() << endl;
                cout << features->size() << endl;
                vt->append_cloud(features, indices, adjacencies, false);
                features->clear();
                indices.clear();
                adjacencies.clear();
            }

            cout << __FILE__ << ", " << __LINE__ << endl;

            last_sweep = sweep_i;
            sweep_counter = 0;
        }

        cout << __FILE__ << ", " << __LINE__ << endl;

        if (features_i->size() < min_segment_features) {
            ++counter;
            ++sweep_counter;
            if (sweep_i >= min_training_sweeps && islast) {
                break;
            }
            continue;
        }

        cout << __FILE__ << ", " << __LINE__ << endl;

        //Eigen::Vector4f point;
        //pcl::compute3DCentroid(*keypoints_i, point);
        //centroids->push_back(PointT());
        //centroids->back().getVector4fMap() = point;
        features->insert(features->end(), features_i->begin(), features_i->end());

        cout << __FILE__ << ", " << __LINE__ << endl;

        IndexT index(sweep_i, counter, sweep_counter);
        for (size_t i = 0; i < features_i->size(); ++i) {
            indices.push_back(index);
        }

        cout << __FILE__ << ", " << __LINE__ << endl;

        ++counter;
        ++sweep_counter;
        if (sweep_i >= min_training_sweeps && islast) {
            break;
        }
    }

    cout << __FILE__ << ", " << __LINE__ << endl;

    // append the rest
    cout << "Appending " << features->size() << " points in " << adjacencies.size() << " groups" << endl;

    if (features->size() > 0) {
        adjacencies.push_back(compute_group_adjacencies(segment_path.parent_path()));
        vt->append_cloud(features, indices, adjacencies, false);
    }

    cout << __FILE__ << ", " << __LINE__ << endl;

    summary.nbr_noise_segments = counter;
    summary.nbr_noise_sweeps = sweep_counter;
    summary.nbr_annotated_segments = 0;
    summary.nbr_annotated_sweeps = 0;

    cout << __FILE__ << ", " << __LINE__ << endl;

    summary.save(vocabulary_path);
    dynamic_object_retrieval::save_vocabulary(*vt, vocabulary_path);
}

// hopefully these sweeps will always be after the previously collected ones
// maybe even iterate through all of this to assert that there are no more?
pair<size_t, size_t> get_offsets_in_data(const boost::filesystem::path& sweep_path,
                                         const boost::filesystem::path& data_path)
{
    // iterate through all of the segment paths, don't load anything
    //dynamic_object_retrieval::convex_feature_cloud_map segment_features(data_path);
    //dynamic_object_retrieval::convex_keypoint_cloud_map segment_keypoints(data_path);
    cout << "Computing vocabulary offsets..." << endl;
    dynamic_object_retrieval::convex_sweep_index_map sweep_indices(data_path);
    dynamic_object_retrieval::convex_segment_map segment_paths(data_path);

    size_t counter = 0;
    size_t sweep_i;
    size_t last_sweep = 0;
    boost::filesystem::path segment_path;
    for (auto tup : dynamic_object_retrieval::zip(sweep_indices, segment_paths)) {

        boost::tie(sweep_i, segment_path) = tup;

        // skips 0, that's ok
        if (sweep_i != last_sweep) {
            // this should actually work, right?
            cout << sweep_path.string() << endl;
            cout << segment_path.parent_path().parent_path().string() << endl;
            if (sweep_path == segment_path.parent_path().parent_path()) {
                cout << "Matching!" << endl;
                return make_pair(sweep_i, counter);
            }
            else {
                cout << "Not matching!" << endl;
            }
        }

        ++counter;
    }

    cout << "Could not find any corresponding indices!" << endl;
    exit(0);
    return make_pair(0, 0);
}

void vocabulary_callback(const std_msgs::String::ConstPtr& msg)
{
    cout << "Received callback with path " << msg->data << endl;
    boost::filesystem::path sweep_xml(msg->data);
    boost::filesystem::path segments_path = sweep_xml.parent_path() / "convex_segments";

    boost::filesystem::path data_path = sweep_xml.parent_path() // room directory
                                                 .parent_path() // patrol run
                                                 .parent_path() // date
                                                 .parent_path() // data root
                                                 .parent_path(); // actual data root in this data set

    if (vt == NULL) {
        vector<string> room_xmls = semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string());
        size_t nbr_segmented_sweeps = 0;
        for (const string& xml : room_xmls) {
            if (!boost::filesystem::exists(boost::filesystem::path(xml).parent_path() / "convex_segments")) {
                break;
            }
            ++nbr_segmented_sweeps;
        }
        if (nbr_segmented_sweeps >= min_training_sweeps + 1) {
            train_vocabulary(data_path);
        }
        std_msgs::String done_msg;
        done_msg.data = msg->data;
        pub.publish(done_msg);
        return;
    }

    cout << __FILE__ << ", " << __LINE__ << endl;

    size_t offset; // same thing here, index among segments, can't take this from vocab, instead parse everything?
    size_t sweep_offset;
    // how do we get this to not require anything
    tie(sweep_offset, offset) = get_offsets_in_data(sweep_xml.parent_path(), data_path);

    cout << __FILE__ << ", " << __LINE__ << endl;

    size_t counter = 0; // this is important to change as its the index within all segments
    size_t sweep_i; // index among sweeps, how do we get this?

    cout << __FILE__ << ", " << __LINE__ << endl;

    HistCloudT::Ptr features(new HistCloudT);
    AdjacencyT adjacencies;
    vector<IndexT> indices;

    cout << __FILE__ << ", " << __LINE__ << endl;

    dynamic_object_retrieval::sweep_convex_segment_cloud_map clouds(sweep_xml.parent_path());
    dynamic_object_retrieval::sweep_convex_feature_cloud_map segment_features(sweep_xml.parent_path());
    dynamic_object_retrieval::sweep_convex_keypoint_cloud_map segment_keypoints(sweep_xml.parent_path());
    cout << __FILE__ << ", " << __LINE__ << endl;
    for (auto tup : dynamic_object_retrieval::zip(segment_features, segment_keypoints, clouds)) {
        HistCloudT::Ptr features_i;
        CloudT::Ptr keypoints_i;
        CloudT::Ptr segment;
        tie(features_i, keypoints_i, segment) = tup;

        cout << __FILE__ << ", " << __LINE__ << endl;

        features->insert(features->end(), features_i->begin(), features_i->end());

        cout << __FILE__ << ", " << __LINE__ << endl;

        IndexT index(sweep_offset + sweep_i, offset + counter, counter);
        for (size_t i = 0; i < features_i->size(); ++i) {
            indices.push_back(index);
        }

        cout << __FILE__ << ", " << __LINE__ << endl;

        ++counter;
    }

    cout << __FILE__ << ", " << __LINE__ << endl;

    if (features->size() > 0) {
        adjacencies.push_back(compute_group_adjacencies(segments_path));
        vt->append_cloud(features, indices, adjacencies, false);
    }

    cout << __FILE__ << ", " << __LINE__ << endl;

    std_msgs::String done_msg;
    done_msg.data = msg->data;
    pub.publish(done_msg);
}

void bypass_callback(const std_msgs::String::ConstPtr& msg)
{
    std_msgs::String done_msg;
    done_msg.data = msg->data;
    pub.publish(done_msg);
}

int main(int argc, char** argv)
{
    ros::init(argc, argv, "retrieval_vocabulary");
    ros::NodeHandle n;

    ros::NodeHandle pn("~");
    pn.param<int>("min_training_sweeps", min_training_sweeps, 20);
    string temp_path;
    pn.param<string>("vocabulary_path", temp_path, "~/.ros/vocabulary");
    vocabulary_path = boost::filesystem::path(temp_path);
    boost::filesystem::create_directory(vocabulary_path);
    bool bypass;
    pn.param<bool>("bypass", bypass, 0);

    // add something to check if the vocabulary is already trained
    if (boost::filesystem::exists(vocabulary_path / "grouped_vocabulary.cereal")) {
        vt = new VocT(vocabulary_path.string());
        vt->set_min_match_depth(3);
        dynamic_object_retrieval::load_vocabulary(*vt, vocabulary_path);
    }
    else {
        vt = NULL;
    }

    pub = n.advertise<std_msgs::String>("/vocabulary_done", 1);

    ros::Subscriber sub;
    if (bypass) {
        sub = n.subscribe("/features_done", 1, bypass_callback);
    }
    else {
        sub = n.subscribe("/features_done", 1, vocabulary_callback);
    }

    ros::spin();

    return 0;
}
