#include "pch.h"
#include <EventAPI.h>
#include <LoggerAPI.h>
#include <MC/Level.hpp>
#include <MC/BlockInstance.hpp>
#include <MC/Block.hpp>
#include <MC/BlockSource.hpp>
#include <MC/Actor.hpp>
#include <MC/Player.hpp>
#include <MC/Item.hpp>
#include <MC/ItemStack.hpp>
#include <MC/ItemStackBase.hpp>
#include <MC/DispenserBlock.hpp>    //发射器
#include <MC/Container.hpp>         //容器
#include "Version.h"
#include <LLAPI.h>
#include <ServerAPI.h>
#include <direct.h>
#include <MC/Types.hpp>
//#include <MC/DispenserBlockActor.hpp>

#include <Nlohmann/json.hpp>

Logger DispenserDestroyBlockLogger(PLUGIN_NAME);

using json = nlohmann::json;

json config = R"(
  {
	"instant_destruction" : false,
    "stay_in_the_Dispenser" : true,
    "consume_durable" : true,
    "destroy":
		{
			"minecraft:iron_pickaxe" : 
                {
                    "minecraft:stone" : 
                        {
                            "dropitem" : "minecraft:cobblestone",
                            "count" : 1
                        },
                    "minecraft:cobblestone" : 
                        {
                            "dropitem" : "",
                            "count" : 1
                        }
                },
            "minecraft:iron_axe" : 
                {
                    "minecraft:log" : 
                        {
                            "dropitem" : "",
                            "count" : 1
                        },
                    "minecraft:log2" : 
                        {
                            "dropitem" : "",
                            "count" : 1
                        }
                }
		}
  }
)"_json;

string configpath = "./plugins/DispenserDestroyBlock/";

inline void CheckProtocolVersion() {
#ifdef TARGET_BDS_PROTOCOL_VERSION
    auto currentProtocol = LL::getServerProtocolVersion();
    if (TARGET_BDS_PROTOCOL_VERSION != currentProtocol)
    {
        logger.warn("Protocol version not match, target version: {}, current version: {}.",
            TARGET_BDS_PROTOCOL_VERSION, currentProtocol);
        logger.warn("This will most likely crash the server, please use the Plugin that matches the BDS version!");
    }
#endif // TARGET_BDS_PROTOCOL_VERSION
}

void AutoUprade(const std::string minebbs_resid);

void PluginInit()
{
    CheckProtocolVersion();
    AutoUprade("4066");

    if (_access(configpath.c_str(), 0) == -1)	//表示配置文件所在的文件夹不存在
    {
        if (_mkdir(configpath.c_str()) == -1)
        {
            //文件夹创建失败
            DispenserDestroyBlockLogger.warn("Directory creation failed, please manually create the plugins/DispenserDestroyBlock directory");
            return;
        }
    }
    std::ifstream f((configpath + "DispenserDestroyBlock.json").c_str());
    if (f.good())								//表示配置文件存在
    {
        f >> config;
        f.close();
    }
    else {
        //配置文件不存在
        std::ofstream c((configpath + "DispenserDestroyBlock.json").c_str());
        c << config.dump(2);
        c.close();
    }
}

// 敲定思路
// 读取JSON文件 某个工具破坏什么方块
// 代码执行过程
// 1.获取发射的物品,读取JSON,工具中是否有这个物品(是否有这个物品的key)
// 2.获取前方的方块,在JSON中判断这个方块是否是这个物品可破坏方块中的一个(在这个物品下 是否有这个方块的key)
// 3.在JSON中获取 使用这个物品破坏这个方块 应该掉落什么方块
// 4.破坏后使物品的特殊值加一
// 另外:如果这个物品在JSON中有配置,但这个方块却不是这个物品可破坏的类型,那不将这个物品发射出来

//铁镐:minecraft:iron_pickaxe
//圆石:minecraft:cobblestone
//铁斧:minecraft:iron_axe
//石头:minecraft:stone
//原木:minecraft:log,minecraft:log2

// a4  发射物品在容器中的位置 0开始
// ret 是否拦截发射 true不发射
THook(bool, "?dispense@Item@@UEBA_NAEAVBlockSource@@AEAVContainer@@HAEBVVec3@@E@Z", Item* thi, BlockSource* a2, Container* a3, int a4, Vec3* a5, unsigned char a6)
{
    auto itemstack = a3->getSlot(a4);
    //发射的物品 名称
    auto itemN = itemstack->getTypeName();
    //发射器对着的方块 名称
    auto blockN = a2->getBlock(a5->toBlockPos()).getTypeName();
    //如果配置文件中对该物品有行为指定
    if (config["destroy"].contains(itemN))
    {
        //如果配置文件指定，发射器对着的方块是允许该发射物破坏的方块
        if (config["destroy"][itemN].contains(blockN))
        {
            bool isdestroy = false;
            if (config["destroy"][itemN][blockN]["dropitem"] == "")
            {
                isdestroy = a2->getLevel().destroyBlock(*a2, a5->toBlockPos(), true);
            }
            else
            {
                isdestroy = a2->getLevel().destroyBlock(*a2, a5->toBlockPos(), false);
                auto item = ItemStack::create(std::string(config["destroy"][itemN][blockN]["dropitem"]), config["destroy"][itemN][blockN]["count"]);
                Level::spawnItem(*a5, a2->getDimensionId(), item);
            }

            //如果破坏成功，并且要求消耗耐久
            if (isdestroy && config["consume_durable"] == true)
            {
                //auto maxduration = a5->getMaxUseDuration();
                auto damage = itemstack->getDamageValue();
                auto maxdamage = itemstack->getMaxDamage();
                if (maxdamage == 0)
                {
                    return true;
                }

                if (damage >= maxdamage)
                {
                    itemstack->remove(1);
                }
                else
                {
                    itemstack->setDamageValue(damage + (short)1);
                    //DispenserDestroyBlockLogger.info("物品特殊值:{0}", (int)(a5->getDamageValue()));
                    //DispenserDestroyBlockLogger.info("最大耐久:{0}", a5->getMaxDamage());
                }
            }

            return true;
        }

        if (config["stay_in_the_Dispenser"])
        {
            return true;
        }
    }
    return original(thi, a2, a3, a4, a5, a6);
}