// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__PluginManager__
#define __ODC__PluginManager__

// STD
#include <map>
#include <memory>
#include <string>
// ODC
#include "Def.h"

namespace odc::core
{
    class CPluginManager
    {
      public:
        // Shared pointer
        using Ptr_t = std::shared_ptr<CPluginManager>;
        // Plugin map <plugin name, cmd>
        using PluginMap_t = std::map<std::string, std::string>;

        void registerPlugin(const std::string& _plugin, const std::string& _cmd);
        std::string execPlugin(const std::string& _plugin,
                               const std::string& _resources,
                               const partitionID_t& _partitionID,
                               runNr_t _runNr) const;
        bool isPluginRegistered(const std::string& _plugin) const;

      private:
        PluginMap_t m_plugins; ///< Plugin map
    };
} // namespace odc::core

#endif /* defined(__ODC__PluginManager__) */
