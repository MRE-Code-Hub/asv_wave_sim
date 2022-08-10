// Copyright (C) 2022  Rhys Mainwaring
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef GZ_RENDERING_OCEANSCENE_HH_
#define GZ_RENDERING_OCEANSCENE_HH_

#include "OceanGeometry.hh"
#include "OceanVisual.hh"

#include <gz/rendering/config.hh>
#include <gz/rendering/Scene.hh>
#include "gz/rendering/Export.hh"

#include <memory>

namespace gz
{
  namespace rendering
  {
    inline namespace GZ_RENDERING_VERSION_NAMESPACE {

    class GZ_RENDERING_VISIBLE OceanScene
    {
      /// \brief Destructor
      public: virtual ~OceanScene() = default;

      /// \brief Create an ocean visual
      public: virtual OceanVisualPtr CreateOceanVisual(ScenePtr _scene) = 0;
    
      /// \brief Create an ocean geometry
      public: virtual OceanGeometryPtr CreateOceanGeometry(ScenePtr _scene) = 0;
    };

    typedef std::shared_ptr<OceanScene> OceanScenePtr;

    }
  }
}
#endif
