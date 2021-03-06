#include <algorithm>

#include <cstdio>
#include <glob.h>

#include <deal.II-pod/extra/extra.h>

namespace POD
{
  using namespace dealii;

  namespace extra
  {
    std::vector<std::string> expand_file_names(const std::string &file_name_glob)
    {
      std::vector<std::string> file_names;
      glob_t glob_result;
      glob(file_name_glob.c_str(), GLOB_TILDE, nullptr, &glob_result);
      for (unsigned int file_name_n = 0; file_name_n < glob_result.gl_pathc;
           ++file_name_n)
        {
          file_names.emplace_back(glob_result.gl_pathv[file_name_n]);
        }
      globfree(&glob_result);
      std::sort(file_names.begin(), file_names.end());
      return file_names;
    }



    bool are_equal(const BlockVector<double> &left,
                   const BlockVector<double> &right,
                   const double               tolerance)
    {
      if (left.n_blocks() != right.n_blocks())
        {
          return false;
        }
      for (unsigned int i = 0; i < left.n_blocks(); ++i)
        {
          if (left.block(i).size() != right.block(i).size())
            {
              return false;
            }
        }

      for (unsigned int i = 0; i < left.n_blocks(); ++i)
        {
          for (unsigned int j = 0; j < left.block(i).size(); ++j)
            {
              if (std::abs(left.block(i)[j] - right.block(i)[j]) > tolerance)
                {
                  return false;
                }
            }
        }

      return true;
    }



    bool are_equal(const FullMatrix<double> &left,
                   const FullMatrix<double> &right,
                   const double              tolerance)
    {
      if (left.m() != right.m() || left.n() != right.n())
        {
          return false;
        }

      for (unsigned int i = 0; i < left.m(); ++i)
        {
          for (unsigned int j = 0; j < left.n(); ++j)
            {
              if (std::abs(left(i, j) - right(i, j)) > tolerance)
                {
                  return false;
                }
            }
        }

      return true;
    }



    bool are_equal(const Vector<double> &left,
                   const Vector<double> &right,
                   const double          tolerance)
    {
      if (left.size() != right.size())
        {
          return false;
        }

      for (unsigned int i = 0; i < left.size(); ++i)
        {
          if (std::abs(left[i] - right[i]) > tolerance)
            {
              return false;
            }
        }

      return true;
    }


    TemporaryFileName::TemporaryFileName()
    {
      std::array<char, 20> character_template;
      std::fill(character_template.begin(), character_template.end() - 7, 'a');
      std::fill(character_template.end() - 7, character_template.end() - 1, 'X');
      character_template.back() = '\0';

      // we don't need the handle so close it immediately
      int handle = mkstemp(character_template.data());
      AssertThrow(handle != -1, ExcInternalError());
      int ierr = close(handle);
      AssertThrow(ierr != -1, ExcInternalError());

      name = std::string(character_template.begin(), character_template.end());
    }

    TemporaryFileName::~TemporaryFileName()
    {
      std::remove(name.c_str());
    }
  }
}
