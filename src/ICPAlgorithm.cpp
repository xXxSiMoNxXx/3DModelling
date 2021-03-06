﻿#include "ICPAlgorithm.h"
#include "FileProcessingH.h"
ICPAlgorithm::ICPAlgorithm()
{
}

ICPAlgorithm::~ICPAlgorithm()
{
}

/* Method 1 of building the 3d model:
it takes each frame and register it with the result. */

void ICPAlgorithm::register_with_result(vector<PointCloudT> &clouds, int iteration, double probability)
{

	PointCloudPtr previous_frameS(new PointCloudT);
	PointCloudPtr all_samples(new PointCloudT);
	PointCloudPtr current(new PointCloudT);
	PointCloudPtr current_sample(new PointCloudT);
	Matrix4f Total_transformation;
	float transformation_errQtoP = 0;
	FileProcessing fp;
	visualization::CloudViewer viewer("PCL OpenNI Viewer");

	get_random_points(clouds[0], *previous_frameS, 1);
	get_random_points(*previous_frameS, *all_samples, probability);

	// Used for accumulating the rigid transformation matrix.
	Total_transformation.setIdentity();


	for (int frame = 1; frame < clouds.size(); ++frame) {
		pcl::console::TicToc time;
		time.tic();
		get_random_points(clouds[frame], *current, 1);
		get_random_points(*current, *current_sample, probability);

		// Transform the source with the last transformation, so it will be near the aligned points.
		transform_pointcloud(*current_sample, *current_sample, Total_transformation);
		transform_pointcloud(*current, *current, Total_transformation);

		cout << "**************************** Full scene done " << (double)frame / (double)(clouds.size() - 1) * 100 << "% ****************************" << endl;
		// ICP STARTS.
		for (int i = 0; i < iteration; ++i) {
			cout << "ICP Iteration " << i << endl;
			// Find nearest neighbor using kdtree of two point clouds.
			PointCloudPtr p(new PointCloudT);
			PointCloudPtr q(new PointCloudT);
			find_nearest_neighbors(*all_samples, *current_sample, *p, *q);

			Matrix4f transformation;
			// Getting the transformation matrix from current frame to the previous frame.
			compute_rigid_transformation(*q, *p, transformation);
			cout << transformation << endl;
			// Calculating MSE.
			transformation_errQtoP = 0;
			for (size_t j = 0; j < q->points.size(); ++j)
				transformation_errQtoP += squaredEuclideanDistance(p->points[j], q->points[j]);
			transformation_errQtoP = transformation_errQtoP / q->points.size();
			cout << "Q to P transformation err " << transformation_errQtoP << endl;

			// Transform source to target given the transformation.
			// Q'=Q*R+T
			transform_pointcloud(*current_sample, *current_sample, transformation);
			transform_pointcloud(*current, *current, transformation);
			// Updating total transformation.
			Total_transformation *= transformation;

		}
		cout << "complete " << time.toc() << " ms" << endl;
		// Merging two point clouds.
		PointCloudPtr merged(new PointCloudT);
		PointCloudPtr current_filtered(new PointCloudT);
		*merged += *previous_frameS;
		filter_points_using_reference_frame(*merged, *current, *current_filtered);
		*merged += *current_filtered;
		previous_frameS = merged;


		viewer.showCloud(previous_frameS);
		PCLPointCloud2::Ptr cloud(new pcl::PCLPointCloud2());
		PCLPointCloud2::Ptr cloud_filtered(new pcl::PCLPointCloud2());
		toPCLPointCloud2(*previous_frameS, *cloud);
		VoxelGrid<PCLPointCloud2> sor;
		sor.setInputCloud(cloud);
		sor.setLeafSize(0.005f, 0.005f, 0.005f);
		sor.filter(*cloud_filtered);
		pcl::fromPCLPointCloud2(*cloud_filtered, *previous_frameS);

		// Merging subsamples.
		PointCloudPtr merged_sample(new PointCloudT);
		PointCloudPtr current_sample_filtered(new PointCloudT);
		*merged_sample += *all_samples;
		filter_points_using_reference_frame(*merged_sample, *current_sample, *current_sample_filtered);
		*merged_sample += *current_sample_filtered;
		all_samples = merged_sample;
	}
	cout << "**************************** Writing the results ****************************" << endl;
	fp.save_point_cloud("FullSciene.ply", *previous_frameS);
}
/* Method 2 of building the 3d model: (not Used)
adding the aligned point cloud to the result. */
void ICPAlgorithm::build_3d_map(vector<PointCloudT>& clouds, int iteration, double probability)
{
	PointCloudPtr result_show(new PointCloudT);
	PointCloudT result;
	PointCloudPtr target(new PointCloudT);
	PointCloudPtr source(new PointCloudT);
	PointCloudPtr source_trans_PointCloud(new PointCloudT);
	Matrix4f last_transformation;
	FileProcessing fp;
	visualization::CloudViewer viewer("PCL OpenNI Viewer");
	last_transformation.setIdentity();
	result += clouds[0];
	//result_show.reset();
	result_show->header = result.header;
	result_show->points.resize(result.points.size());
	*source_trans_PointCloud = result;
	*target = clouds[0];
	for (size_t i = 1; i < clouds.size(); i++)
	{
		cout << "**************************** Full scene done " << (double)i / (double)(clouds.size() - 1) * 100 << "% ****************************" << endl;

		*source = clouds[i];
		transform_pointcloud(*source, *source, last_transformation);
		aligning_two_pointcloud(*source, *target, *source_trans_PointCloud, iteration, probability, last_transformation);
		*target = *source_trans_PointCloud;

		//PointCloudPtr current_sample_filtered(new PointCloudT);
		//filter_points_using_reference_frame(result, *source_trans_PointCloud, *current_sample_filtered);
		//result += *current_sample_filtered;

		result += *source_trans_PointCloud;
		*result_show = result;
		viewer.showCloud(result_show);
		PCLPointCloud2::Ptr cloud(new pcl::PCLPointCloud2());
		PCLPointCloud2::Ptr cloud_filtered(new pcl::PCLPointCloud2());
		toPCLPointCloud2(result, *cloud);
		VoxelGrid<PCLPointCloud2> sor;
		sor.setInputCloud(cloud);
		//sor.setLeafSize(0.09f, 0.09f, 0.09f);
		sor.setLeafSize(0.005f, 0.005f, 0.005f);
		sor.filter(*cloud_filtered);
		pcl::fromPCLPointCloud2(*cloud_filtered, result);

	}
	//get_random_points(result, result, 0.01);
	fp.save_point_cloud("FullSciene.ply", result);

}

/*Method 3 of building the 3d model: (not Used)
This method takes each frame and register it with the previous frame, 
we didn't use it since if the transformation wasn't good enough the mistake is accumulate very fast. */
void ICPAlgorithm::aligning_two_pointcloud(const PointCloudT & src, const PointCloudT & tgt, PointCloudT & aligned, int iteration, double probability, Matrix4f &total_transformation)
{
	PointCloudPtr sub_source(new PointCloudT);
	PointCloudPtr temp_source(new PointCloudT);
	PointCloudPtr temp_target(new PointCloudT);
	float transformation_errQtoP = 100;
	get_random_points(tgt, aligned, 1);

	// For the total transformation matrix.
	total_transformation = total_transformation.setIdentity();

	transform_pointcloud(tgt, *temp_target, total_transformation);
	transform_pointcloud(src, *temp_source, total_transformation);
	pcl::console::TicToc time;
	time.tic();
	get_random_points(*temp_source, *sub_source, probability);
	// Transform the source with the last transformation so it will be near the aligned points.
	transform_pointcloud(*sub_source, *sub_source, total_transformation);

	PointCloudPtr p(new PointCloudT);
	PointCloudPtr q(new PointCloudT);

	// ICP iteration START here.
	for (int i = 0; transformation_errQtoP > 0.00002 && i < iteration; ++i) {
		cout << "ICP Iteration " << i << endl;
		// Find nearest neighbor using kdtree of two point clouds.
		find_nearest_neighbors(aligned, *sub_source, *p, *q);

		// Getting the  transformation matrix from current frame to the previous frame.
		Matrix4f transformQtoP;
		compute_rigid_transformation(*q, *p, transformQtoP);
		cout << transformQtoP << endl;
		transformation_errQtoP = 0;
		for (size_t j = 0; j < q->points.size(); ++j)
			transformation_errQtoP += squaredEuclideanDistance(p->points[j], q->points[j]);
		transformation_errQtoP = transformation_errQtoP / q->points.size();
		cout << "Q to P transformation err " << transformation_errQtoP << endl;

		// Transform source to target given the transformation.
		// Q'=Q*R+T
		transform_pointcloud(*sub_source, *sub_source, transformQtoP);
		transform_pointcloud(*temp_source, *temp_source, transformQtoP);
		// updating the total transformation.
		total_transformation *= transformQtoP;
		//if (transformation_errQtoP<0.00003)
		//	break;

	}
	cout << "complete " << time.toc() << " ms" << endl;
	// Merging two point clouds.
	PointCloudPtr combined(new PointCloudT);
	*combined += *temp_target;
	*combined += *temp_source;

	// Preparing the return value.

	aligned = *temp_source;
	// Save the result as ply file.

	//FileProcessing fp;
	//fp.save_point_cloud("output.ply", *combined);


}
/* The function transform a given point cloud using a given transformation. */
void ICPAlgorithm::transform_pointcloud(const PointCloudT & in, PointCloudT & out, const Matrix4f & transformation)
{
	Matrix3f rotation;
	Vector3f translation;

	// Initial the output point cloud if both point cloud are not the same.
	if (&in != &out)
	{
		out.header = in.header;
		out.width = in.width;
		out.height = in.height;
		out.is_dense = in.is_dense;
		out.points.reserve(out.points.size());
		out.points.assign(in.points.begin(), in.points.end());
	}

	// Extracting the translation and rotation matrix out of 4x4 Transformation Matrix.
	extract_rotation_and_translation(transformation, rotation, translation);
	// Applying the transformation on the point cloud.
	for (size_t i = 0; i < out.points.size(); ++i)
		out.points[i].getVector3fMap() = rotation * in.points[i].getVector3fMap() + translation;
}

/* The function compute the transformation of the source frame to the target frame and it returns the transformation. */
void ICPAlgorithm::compute_rigid_transformation(const PointCloudT &source, const PointCloudT &target, Matrix4f &transformation)
{
	Vector4f centroid_source, centroid_target;
	MatrixXf subtracted_centroid_source, subtracted_centroid_target;
	Matrix3f covarianse, U, V, rotation;
	Vector3f rotated_centroid;
	// Initial the transformation as identity.
	transformation.setIdentity();

	// Find the centroids of the given point cloud.
	calculate_centroid(source, centroid_source);
	calculate_centroid(target, centroid_target);

	// Eliminate the centroid from the point clouds
	subtract_centroid(source, centroid_source, subtracted_centroid_source);

	subtract_centroid(target, centroid_target, subtracted_centroid_target);

	// Compute Covariance matrix subtracted_mean_source * subtracted_mean_target'.
	covarianse = (subtracted_centroid_source * subtracted_centroid_target.transpose()).topLeftCorner<3, 3>();

	// Apply SVD on Covariance matrix.
	JacobiSVD<Matrix3f> SVD(covarianse, ComputeFullU | ComputeFullV);
	U = SVD.matrixU();
	V = SVD.matrixV();

	// If reflection detected.
	if (U.determinant() * V.determinant() < 0)
		for (int z = 0; z < 3; ++z)
			V(z, 2) *= -1;

	// The rotation matrix: 𝑅 = 𝑉∗𝑈^𝑇.
	rotation = V * U.transpose();

	// Return the correct transformation
	transformation.topLeftCorner<3, 3>() = rotation;
	rotated_centroid = rotation * centroid_source.head<3>();
	// The translation matrix: 𝑡 =  𝑐𝑒𝑛𝑡𝑟𝑜𝑖𝑑_(𝑃^∗)− 𝑅∗𝑐𝑒𝑛𝑡𝑟𝑜𝑖𝑑_𝑄.
	transformation.block <3, 1>(0, 3) = centroid_target.head<3>() - rotated_centroid;
}

/* This function takes two point clouds and it finds the closet point in order to find the corresponding points, using the kdtree data structure. */

void ICPAlgorithm::find_nearest_neighbors(const PointCloudT & prev_frame, const PointCloudT & curr_frame, PointCloudT & p, PointCloudT & q)
{
	int K = 1;
	KdTreeFLANN<PointT> kdtree;
	PointCloudPtr prev(new PointCloudT);
	PointCloudPtr curr(new PointCloudT);
	PointT search_point, matched_point;
	vector<int> pointIdxNKNSearch(K);
	vector<float> pointNKNSquaredDistance(K);

	*prev = prev_frame;
	*curr = curr_frame;

	if (prev_frame.points.size() < curr_frame.points.size())
	{
		kdtree.setInputCloud(curr);

		if (&p != &prev_frame && &q != &curr_frame)
		{

			p.header = prev_frame.header;
			p.width = prev_frame.width;
			p.height = prev_frame.height;
			p.is_dense = prev_frame.is_dense;
			p.points.reserve(p.points.size());
			p.points.assign(prev_frame.points.begin(), prev_frame.points.end());
			q.header = prev_frame.header;
			q.width = prev_frame.width;
			q.height = prev_frame.height;
			q.is_dense = prev_frame.is_dense;
			q.points.reserve(q.points.size());
			q.points.assign(prev_frame.points.begin(), prev_frame.points.end());
		}


		for (size_t i = 0; i < prev_frame.points.size(); ++i)
		{
			search_point = prev->points[i];
			p.points[i].x = search_point.x;
			p.points[i].y = search_point.y;
			p.points[i].z = search_point.z;

			kdtree.nearestKSearch(search_point, K, pointIdxNKNSearch, pointNKNSquaredDistance);
			matched_point = curr->points[pointIdxNKNSearch[0]];
			q.points[i].x = matched_point.x;
			q.points[i].y = matched_point.y;
			q.points[i].z = matched_point.z;

		}
	}
	else
	{
		kdtree.setInputCloud(prev);
		if (&p != &prev_frame && &q != &curr_frame)
		{

			p.header = curr_frame.header;
			p.width = curr_frame.width;
			p.height = curr_frame.height;
			p.is_dense = curr_frame.is_dense;
			p.points.reserve(p.points.size());
			p.points.assign(curr_frame.points.begin(), curr_frame.points.end());
			q.header = curr_frame.header;
			q.width = curr_frame.width;
			q.height = curr_frame.height;
			q.is_dense = curr_frame.is_dense;
			q.points.reserve(q.points.size());
			q.points.assign(curr_frame.points.begin(), curr_frame.points.end());
		}


		for (size_t i = 0; i < curr_frame.points.size(); ++i)
		{
			search_point = curr->points[i];
			q.points[i].x = search_point.x;
			q.points[i].y = search_point.y;
			q.points[i].z = search_point.z;

			kdtree.nearestKSearch(search_point, K, pointIdxNKNSearch, pointNKNSquaredDistance);
			matched_point = prev->points[pointIdxNKNSearch[0]];
			p.points[i].x = matched_point.x;
			p.points[i].y = matched_point.y;
			p.points[i].z = matched_point.z;

		}
	}

}
/* This function extract the rotation and the translation matrix out of 4x4 transformation matrix. */
void ICPAlgorithm::extract_rotation_and_translation(const Matrix4f &transformation, Matrix3f &rotation, Vector3f &translation)
{
	rotation = transformation.block<3, 3>(0, 0);
	translation = transformation.block<3, 1>(0, 3);
}
/* This function build 4x4 transformation matrix given the rotation 3x3 matrix and the translation vector 3x1. */
void ICPAlgorithm::get_transform_matrix(const Matrix3f & rotation, const Vector3f & translation, Matrix4f &transformation)
{
	transformation.setIdentity();
	transformation.block<3, 3>(0, 0) = rotation;
	transformation.block<3, 1>(0, 3) = translation;
}
/* This function choose random points from the point cloud with given probability, and it returns the subsample. */
void ICPAlgorithm::get_random_points(const PointCloudT & cloud_in, PointCloudT & subsample, double probability)
{
	int j = 0;
	std::vector<int> index;

	subsample.is_dense = true; // there are non NANs in the subsample cloud.

	// Initial the subsample point cloud if both point cloud are not the same.
	if (&cloud_in != &subsample)
	{
		subsample.header = cloud_in.header;
		subsample.points.resize(cloud_in.points.size());
	}
	// Initialize the index size.
	index.resize(cloud_in.points.size());
	srand(time(NULL));
	for (size_t i = 0; i < subsample.points.size(); ++i)
	{

		if (((rand() % 100) / 100.0) < probability)
		{
			subsample.points[j] = cloud_in.points[i];
			index[j] = i;
			j++;
		}

	}

	// Update subsample to correct size.
	subsample.points.resize(j);
	index.resize(j);
	subsample.height = 1;
	subsample.width = j;

	cout << "before subsample: " << cloud_in.points.size() << endl;
	cout << "subsample: " << subsample.points.size() << endl;
}
/* This function subtract the centroid of the pointcloud from each point in the given point cloud */
void ICPAlgorithm::subtract_centroid(const PointCloudT &cloud_input, const Vector4f &centroid, MatrixXf &cloud_output)
{
	size_t points_number;

	points_number = cloud_input.points.size();
	cloud_output = MatrixXf::Zero(4, points_number);        // Initial the output matrix.

	for (size_t i = 0; i < points_number; ++i)
		cloud_output.block<4, 1>(0, i) = cloud_input.points[i].getVector4fMap() - centroid;

	// Reset the forth dimension.
	cloud_output.block(3, 0, 1, points_number).setZero();
}
/* This function calculate the centroid of the point cloud. */
void ICPAlgorithm::calculate_centroid(const PointCloudT &cloud_input, Vector4f &centroid)
{
	centroid.setZero(); //initial centroid.
	if (cloud_input.points.empty()) // If cloud is empty.
		return;

	for (size_t i = 0; i < cloud_input.points.size(); ++i)
		centroid += cloud_input.points[i].getVector4fMap();
	centroid[3] = 0; // Reset the forth dimension.
	centroid = centroid / cloud_input.points.size();
}

/* This function is responsible of merging the point cloud together that 
adds only the points which are not exist in a reference frame ,(its not used). */

void ICPAlgorithm::filter_points_using_reference_frame(const PointCloudT & cloud_reference, const PointCloudT & cloud_input, PointCloudT & cloud_out)
{
	int j = 0;
	std::vector<int> index;
	double min_x, max_x, min_y, max_y, delta;
	delta = 0.1;
	cloud_out.is_dense = true; // There are non NANs in the subsample cloud.

	// Initial the subsample point cloud if both point cloud are not the same.
	if (&cloud_input != &cloud_out)
	{
		cloud_out.header = cloud_input.header;
		cloud_out.points.resize(cloud_input.points.size());
	}
	// Initialize the index size.
	index.resize(cloud_input.points.size());
	srand(time(NULL));
	min_x = 100.0;
	min_y = 100.0;
	max_x = -100.0;
	max_y = -100.0;

	for (size_t i = 0; i < cloud_reference.points.size(); ++i)
	{

		if (cloud_reference.points[i].x < min_x)
			min_x = cloud_reference.points[i].x;
		if (cloud_reference.points[i].y < min_y)
			min_y = cloud_reference.points[i].y;

		if (cloud_reference.points[i].x > max_x)
			max_x = cloud_reference.points[i].x;
		if (cloud_reference.points[i].y > max_y)
			max_y = cloud_reference.points[i].y;

	}

	for (size_t i = 0; i < cloud_input.points.size(); ++i)
	{

		if (cloud_input.points[i].x >= max_x - delta || cloud_input.points[i].y >= max_y - delta || cloud_input.points[i].x <= min_x + delta || cloud_input.points[i].y <= min_y + delta)
		{
			cloud_out.points[j] = cloud_input.points[i];
			index[j] = i;
			j++;
		}

	}

	// Update subsample to correct size.
	cloud_out.points.resize(j);
	index.resize(j);
	cloud_out.height = 1;
	cloud_out.width = j;
	cout << "refernce model: " << cloud_reference.points.size() << endl;
	cout << "transformed cloud size: " << cloud_input.points.size() << endl;
	cout << "filtered points size: " << cloud_out.points.size() << endl;
}
