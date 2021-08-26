/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include <sstream>
#include <cstdlib>
#include <gdal_priv.h>
#include <gdal_utils.h>

#include "thumbs.h"

#include <epttiler.h>
#include <pointcloud.h>
#include <tiler.h>

#include "exceptions.h"
#include "hash.h"
#include "utils.h"
#include "userprofile.h"
#include "dbops.h"
#include "mio.h"

namespace ddb{

fs::path getThumbFromUserCache(const fs::path &imagePath, int thumbSize, bool forceRecreate){
    if (std::rand() % 1000 == 0) cleanupThumbsUserCache();
    if (!fs::exists(imagePath)) throw FSException(imagePath.string() + " does not exist");

    const fs::path outdir = UserProfile::get()->getThumbsDir(thumbSize);
    io::Path p = imagePath;
    const fs::path thumbPath = outdir / getThumbFilename(imagePath, p.getModifiedTime(), thumbSize);
    return generateThumb(imagePath, thumbSize, thumbPath, forceRecreate);
}

bool supportsThumbnails(EntryType type){
    return type == Image || type == GeoImage || type == GeoRaster;
}

void generateThumbs(const std::vector<std::string> &input, const fs::path &output, int thumbSize, bool useCrc){
    if (input.size() > 1) io::assureFolderExists(output);
    const bool outputIsFile = input.size() == 1 && io::Path(output).checkExtension({"jpg", "jpeg"});

    const std::vector<fs::path> filePaths = std::vector<fs::path>(input.begin(), input.end());

    for (auto &fp : filePaths){
        LOGD << "Parsing entry " << fp.string();

        const EntryType type = fingerprint(fp);
        io::Path p(fp);

        // NOTE: This check is looking pretty ugly, maybe move "ept.json" in a const?
        if (supportsThumbnails(type) || fp.filename() == "ept.json") {
            fs::path outImagePath;
            if (useCrc){
                outImagePath = output / getThumbFilename(fp, p.getModifiedTime(), thumbSize);
            }else if (outputIsFile){
                outImagePath = output;
            }else{
                outImagePath = output / fs::path(fp).replace_extension(".jpg").filename();
            }
            std::cout << generateThumb(fp, thumbSize, outImagePath, true).string() << std::endl;
        }else{
            LOGD << "Skipping " << fp;
        }
    }
}


fs::path getThumbFilename(const fs::path &imagePath, time_t modifiedTime, int thumbSize){
    // Thumbnails are JPG files idenfitied by:
    // CRC64(imagePath + "*" + modifiedTime + "*" + thumbSize).jpg
    std::ostringstream os;
    os << imagePath.string() << "*" << modifiedTime << "*" << thumbSize;
    return fs::path(Hash::strCRC64(os.str()) + ".jpg");
}

void generateImageThumb(const fs::path& imagePath, int thumbSize, const fs::path& outImagePath) {

    // Compute image with GDAL otherwise
    GDALDatasetH hSrcDataset = GDALOpen(imagePath.string().c_str(), GA_ReadOnly);

    if (!hSrcDataset)
        throw GDALException("Cannot open " + imagePath.string() + " for reading");

    const int width = GDALGetRasterXSize(hSrcDataset);
    const int height = GDALGetRasterYSize(hSrcDataset);
    int targetWidth;
    int targetHeight;

    if (width > height){
        targetWidth = thumbSize;
        targetHeight = static_cast<int>((static_cast<float>(thumbSize) / static_cast<float>(width)) * static_cast<float>(height));
    } else {
        targetHeight = thumbSize;
        targetWidth = static_cast<int>((static_cast<float>(thumbSize) / static_cast<float>(height)) * static_cast<float>(width));
    }

    char** targs = nullptr;
    targs = CSLAddString(targs, "-outsize");
    targs = CSLAddString(targs, std::to_string(targetWidth).c_str());
    targs = CSLAddString(targs, std::to_string(targetHeight).c_str());

    targs = CSLAddString(targs, "-ot");
    targs = CSLAddString(targs, "Byte");

    targs = CSLAddString(targs, "-scale");

    targs = CSLAddString(targs, "-co");
    targs = CSLAddString(targs, "WRITE_EXIF_METADATA=NO");

    // Max 3 bands + alpha
    if (GDALGetRasterCount(hSrcDataset) > 4){
        targs = CSLAddString(targs, "-b");
        targs = CSLAddString(targs, "1");
        targs = CSLAddString(targs, "-b");
        targs = CSLAddString(targs, "2");
        targs = CSLAddString(targs, "-b");
        targs = CSLAddString(targs, "3");
    }

    CPLSetConfigOption("GDAL_PAM_ENABLED", "NO"); // avoid aux files for PNG tiles
    CPLSetConfigOption("GDAL_ALLOW_LARGE_LIBJPEG_MEM_ALLOC", "YES"); // Avoids ERROR 6: Reading this image would require libjpeg to allocate at least 107811081 bytes

    GDALTranslateOptions* psOptions = GDALTranslateOptionsNew(targs, nullptr);
    CSLDestroy(targs);
    GDALDatasetH hNewDataset = GDALTranslate(outImagePath.string().c_str(),
                                             hSrcDataset,
                                             psOptions,
                                             nullptr);
    GDALTranslateOptionsFree(psOptions);

    GDALClose(hNewDataset);
    GDALClose(hSrcDataset);

}

void generatePointCloudThumb(const fs::path &eptPath, int thumbSize,
                        const fs::path &outImagePath) {

    LOGD << "Generating point cloud thumb";

    EptTiler tiler(eptPath.string(), outImagePath.string(), thumbSize);

    const auto box = tiler.getMinMaxZ();

    LOGD << "Box [" << box.min << "; " << box.max; 

    const int z = 20;

    const auto coords = tiler.getMinMaxCoordsForZ(z);

    LOGD << "Coords Max (" << coords.max.x << "; " << coords.max.y << ")"; 
    LOGD << "Coords Min (" << coords.min.x << "; " << coords.min.y << ")"; 
        
    const auto res = tiler.tile(z, (coords.min.x + coords.max.x) / 2, (coords.min.y + coords.max.y) / 2);

    LOGD << "Res = " << res;

}

// imagePath can be either absolute or relative and it's up to the user to
// invoke the function properly as to avoid conflicts with relative paths
fs::path generateThumb(const fs::path &imagePath, int thumbSize, const fs::path &outImagePath, bool forceRecreate){
    if (!exists(imagePath)) throw FSException(imagePath.string() + " does not exist");

    // Check existance of thumbnail, return if exists
    if (exists(outImagePath) && !forceRecreate){
        return outImagePath;
    }

    LOGD << "ImagePath = " << imagePath;
    LOGD << "OutImagePath = " << outImagePath;
    LOGD << "Size = " << thumbSize;

    if (imagePath.filename() == "ept.json")
        generatePointCloudThumb(imagePath, thumbSize, outImagePath);
    else
        generateImageThumb(imagePath, thumbSize, outImagePath);

    return outImagePath;
}

void cleanupThumbsUserCache(){
    LOGD << "Cleaning up thumbs user cache";

    const time_t threshold = utils::currentUnixTimestamp() - 60 * 60 * 24 * 5; // 5 days
    const fs::path thumbsDir = UserProfile::get()->getThumbsDir();
    std::vector<fs::path> cleanupDirs;

    // Iterate size directories
    for(auto sd = fs::recursive_directory_iterator(thumbsDir);
            sd != fs::recursive_directory_iterator();
            ++sd ){
        fs::path sizeDir = sd->path();
        if (is_directory(sizeDir)){
            for(auto t = fs::recursive_directory_iterator(sizeDir);
                    t != fs::recursive_directory_iterator();
                    ++t ){
                fs::path thumb = t->path();
                if (io::Path(thumb).getModifiedTime() < threshold){
                    if (fs::remove(thumb)) LOGD << "Cleaned " << thumb.string();
                    else LOGD << "Cannot clean " << thumb.string();
                }
            }

            if (is_empty(sizeDir)){
                // Remove directory too
                cleanupDirs.push_back(sizeDir);
            }
        }
    }

    for (auto &d : cleanupDirs){
        if (fs::remove(d)) LOGD << "Cleaned " << d.string();
        else LOGD << "Cannot clean " << d.string();
    }
}

}
