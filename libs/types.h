/* Copyright 2019 MasseranoLabs LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */
#ifndef TYPES_H
#define TYPES_H

#include <string>

namespace entry{

enum Type { Undefined = 0, Directory = 1, Generic, GeoImage, GeoRaster, PointCloud };

std::string typeToHuman(Type t);

}

#endif // TYPES_H
