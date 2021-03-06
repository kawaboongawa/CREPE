#include "database_handler.hh"

namespace crepe
{
	Database::Database(filter::FilterHandler filter)
		: chessmans_(std::vector<Chessman>())
		, piece_name_(std::vector<std::string>
	{"king", "queen", "knight", "bishop", "rook", "pawn"})
		, filter_(filter)
	{
		chessmans_.push_back(Chessman(chesspiece::KING));
		chessmans_.push_back(Chessman(chesspiece::QUEEN));
		chessmans_.push_back(Chessman(chesspiece::KNIGHT));
		chessmans_.push_back(Chessman(chesspiece::BISHOP));
		chessmans_.push_back(Chessman(chesspiece::ROOK));
		chessmans_.push_back(Chessman(chesspiece::PAWN));
		init_database();
	}

	Database::~Database()
	{}

	static std::vector<cv::Point> compute_equal_length_points(const std::vector<cv::Point>& arr, int n)
	{
		int size = arr.size();
		std::vector<cv::Point> dst(n);
		float point_dist = static_cast<float>(size) / static_cast<float>(n);
		int index = 0;
		float sum = 0;
		for (int i = 0; i < n; i++)
		{
			dst[i] = arr[static_cast<int>(sum)];
			sum += point_dist;
		}
		return dst;
	}

	static std::vector<std::vector<cv::Point>> normalize_shapes(const std::vector<std::vector<cv::Point>>& contours)
	{
		std::vector<std::vector<cv::Point>> dst;
		for (int i = 0; i < contours.size(); i++)
		{
			if (contours[i].size() > 256)
				dst.push_back(compute_equal_length_points(contours[i], 256));
		}
		return dst;
	}

	int findMaxIndex(const std::vector<std::vector<cv::Point>>& arr)
	{
		int index = 0;
		for (int i = 0; i < arr.size(); i++)
		{
			if (arr[i].size() > arr[index].size())
				index = i;
		}
		return index;
	}

	std::shared_ptr<FourierDescriptor> Database::get_descriptor(const std::string& path)
	{

		std::vector<std::vector<cv::Point>> sh_contours;
		cv::Mat src = cv::imread(path);
		cv::Mat canny;
		GpuMat srcdev;
		srcdev.upload(src);
		GpuMat res = filter_.compute_edges(srcdev);
		res.download(canny);
		//cv::imshow(path, canny);
		//cv::waitKey(0);
	
		cv::findContours(canny, sh_contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE, cv::Point(0, 0));
		std::vector<std::vector<cv::Point>> contours = normalize_shapes(sh_contours);
		if (contours.empty())
			return nullptr;
		
		int index = findMaxIndex(contours);
		size_t size = contours[index].size();

		ushort2* edges = new ushort2[size];
		for (int i = 0; i < size; i++)
		{
			edges[i].x = contours[index][i].x;
			edges[i].y = contours[index][i].y;
		}
		std::shared_ptr<FourierDescriptor> fd = std::make_shared<FourierDescriptor>(edges, size);
		delete[] edges;

		fd->compute_descriptors();
		return fd;
	}

	std::string Database::match_object(const FourierDescriptor& fd, uint ndesc)
	{
		int index = 0;
		float score = FLT_MAX;
		for (int i = 0; i < chessmans_.size(); i++)
		{
			std::string name = piece_name_[i];
			std::vector<std::shared_ptr<FourierDescriptor>> vec_ptr = chessmans_[i].get_descriptors();
			for (int j = 0; j < vec_ptr.size(); j++)
			{
				if (vec_ptr[j] == nullptr)
					continue;
				float tmp = vec_ptr[j]->compare_descriptors(fd, ndesc);
				if (score > tmp)
				{
					score = tmp;
					index = i;
				}
			}
		}
		if (score < 0.1f)
			return piece_name_[index];
		else
			return "unknown";
	}


	inline std::wstring stringTowstring(const std::string &s)
	{
		return std::wstring(s.begin(), s.end());
	}

	inline std::string wstringTostring(const std::wstring &ws)
	{
		return std::string(ws.begin(), ws.end());
	}


	void Database::init_database()
	{
		WIN32_FIND_DATA data;
		HANDLE hFind;

		for (size_t i = 0; i < chessmans_.size(); i++)
		{
			DIR *dir;
			struct dirent *ent;
			std::string path = "..\\database\\" + piece_name_[i];
			if ((dir = opendir(path.c_str())) != NULL) {
				/* print all the files and directories within directory */
				while ((ent = readdir(dir)) != NULL) {
					std::string filename = ent->d_name;
					if (filename == "." || filename == "..")
						continue;
					std::string fullpath = path + "\\" + filename;
					std::shared_ptr<FourierDescriptor> fd = get_descriptor(fullpath);
					chessmans_[i].add_descriptor(fd);
				}
				closedir(dir);
			}
			else {
				/* could not open directory */
				perror(path.c_str());
				return;
			}
		}
	}
}