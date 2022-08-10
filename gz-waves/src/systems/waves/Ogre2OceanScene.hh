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

#ifndef GZ_RENDERING_OGRE2_OCEANSCENE_HH_
#define GZ_RENDERING_OGRE2_OCEANSCENE_HH_

#include "OceanScene.hh"

#include <gz/rendering/config.hh>
#include <gz/rendering/Scene.hh>
#include "gz/rendering/Export.hh"

#include <gz/rendering/ogre2/Export.hh>

namespace gz
{
  namespace rendering
  {
    inline namespace GZ_RENDERING_VERSION_NAMESPACE {

    class GZ_RENDERING_OGRE2_VISIBLE Ogre2OceanScene :
        public OceanScene
    {
      /// \brief Constructor
      public: Ogre2OceanScene();

      /// \brief Destructor
      public: virtual ~Ogre2OceanScene();

      /// \brief Create an ocean visual
      public: virtual OceanVisualPtr CreateOceanVisual(
          ScenePtr _scene) override;
    
      /// \brief Create an ocean geometry
      public: virtual OceanGeometryPtr CreateOceanGeometry(
          ScenePtr _scene) override;

      private: unsigned int objId{50000};
    };

    }
  }
}
#endif