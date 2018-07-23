// Copyright (c) 2012-2018 The Elastos Open Source Project
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sstream>
#include <boost/filesystem.hpp>

#include "SDK/Common/Log.h"
#include "IdCache.h"

using namespace Elastos::ElaWallet;

namespace Elastos {
	namespace DID {

		IdCache::IdCache(const boost::filesystem::path &path) {

			Log::getLogger()->info("ElastosID IdCache open path {}", path.string());

			boost::filesystem::path parentPath = path.parent_path();

			if (!parentPath.empty() && !boost::filesystem::exists(parentPath)) {
				Log::getLogger()->warn("ElastosID IdCache directory parentPath \"{}\" do not exist", parentPath.string());
				if (!boost::filesystem::create_directories(parentPath)) {
					Log::getLogger()->error("ElastosID IdCache create directory parentPath \"{}\" error", parentPath.string());
				}
			}

			leveldb::Options opt;
			opt.create_if_missing = true;
//			opt.filter_policy = NewBloomFilterPolicy(10);
			leveldb::Status s = leveldb::DB::Open(opt, path.string(), &_db);
			if (!s.ok()) {
				Log::getLogger()->error("ElastosID leveldb open path {} error: {}", path.string(), s.ToString());
				_db = nullptr;
			}
		}

		IdCache::IdCache() {
			_db = nullptr;
		}

		IdCache::~IdCache() {
			if (_db) {
				delete _db;
				_db = nullptr;
			}
		}

		bool IdCache::Put(const std::string &id, const nlohmann::json &idJson) {
			Log::getLogger()->error("ElastosID IdCache::Put 1  begin _db->Put id{} idJson{}", id,idJson.dump());

			for (nlohmann::json::const_iterator it = idJson.begin(); it != idJson.end(); it++) {
				if (!Put(id, it.key(), it.value())) {
					return false;
				}
			}

			return true;
		}

		bool IdCache::Put(const std::string &id, const std::string &path, const nlohmann::json &pathJson) {
			Log::getLogger()->error("ElastosID IdCache::Put 3  begin _db->Put id{} path{} idJson{}", id, path,pathJson.dump());

			for (nlohmann::json::const_iterator it = pathJson.begin(); it != pathJson.end(); it++) {
				uint32_t blockHeight = (uint32_t)std::stoi(it.key(), nullptr, 10);
				if (!Put(id, path, blockHeight, it.value())) {
					return false;
				}
			}

			return true;
		}

		bool IdCache::Put(const std::string &id, const std::string &path, uint32_t blockHeight, const nlohmann::json &blockHeighJson) {
			if (!Initialized()) {
				return false;
			}

			nlohmann::json idJson;
			std::string value;
			leveldb::Status s = _db->Get(leveldb::ReadOptions(), id, &value);
			if (!s.ok()) {
				if (!s.IsNotFound()) {
					Log::getLogger()->error("IDCache get id={} error: {}", id, s.ToString());
					return false;
				}
			} else {
				idJson = nlohmann::json::parse(value);
			}

			std::ostringstream oss;
			oss << blockHeight;

			nlohmann::json pathJson;
			if (idJson.find(path) != idJson.end()) {
				pathJson = idJson[path];
			}

			pathJson[oss.str()] = blockHeighJson;

			idJson[path] = pathJson;

			Log::getLogger()->error("ElastosID IdCache::Put _db->Put id{}  idJson{}", id, idJson.dump());
			s = _db->Put(leveldb::WriteOptions(), id, idJson.dump());


			if (!s.ok()) {
				Log::getLogger()->error("IDCache put id={} error: {}", id, s.ToString());
				return false;
			}

			return true;
		}

		nlohmann::json IdCache::GetAllKey(){

			Log::getLogger()->warn("GetAllKey begin");
			if (!Initialized()) {
				return nlohmann::json();
			}

			//test1
			//nlohmann::json::array_t a;
			//nlohmann::json allkeyJson(a);

			//test 2
			nlohmann::json allkeyJson;
			Log::getLogger()->warn("GetAllKey before _db->NewIterator");

			boost::mutex::scoped_lock lock(_lockMutex);

			leveldb::Iterator* it = _db->NewIterator(leveldb::ReadOptions());

			//Log::getLogger()->warn("GetAllKey before for");
			std::vector<std::string> lostrs;

			for (it->SeekToFirst(); it->Valid(); it->Next()) {
				//std::cout << it->key().ToString() << ": "  << it->value().ToString() << std::endl;
				Log::getLogger()->warn("it->key() {}", it->key().ToString());
				lostrs.push_back(it->key().ToString());
				//allkeyJson.push_back(lostr.c_str());
			}
			//Log::getLogger()->warn("GetAllKey before delete ");

			assert(it->status().ok());  // Check for any errors found during the scan
			Log::getLogger()->warn("GetAllKey before delete it with new bee version 2");
			delete it;

			std::for_each(lostrs.begin(), lostrs.end(), [&allkeyJson](const std::string &str){
				allkeyJson.push_back(str);
			});

			Log::getLogger()->warn("GetAllKey end allkeyJson {}", allkeyJson.dump());
			return allkeyJson;
		}

		nlohmann::json IdCache::Get(const std::string &id) const {
			if (!Initialized()) {
				return nlohmann::json();
			}

			std::string value;
			leveldb::Status s = _db->Get(leveldb::ReadOptions(), id, &value);
			if (!s.ok()) {
				Log::getLogger()->error("IDCache get id={} error: {}", id, s.ToString());
				return nlohmann::json();
			}

			return nlohmann::json::parse(value);
		}

		nlohmann::json IdCache::Get(const std::string &id, const std::string &path) const {
			if (!Initialized()) {
				return nlohmann::json();
			}

			std::string value;
			leveldb::Status s = _db->Get(leveldb::ReadOptions(), id, &value);
			if (!s.ok()) {
				Log::getLogger()->error("IDCache get id={} error: {}", id, s.ToString());
				return nlohmann::json();
			}

			nlohmann::json idJson = nlohmann::json::parse(value);
			if (idJson.find(path) == idJson.end()) {
				Log::getLogger()->error("IDCache get id={}, path={} error: not found", id, path);
				return nlohmann::json();
			}

			return idJson[path];
		}

		nlohmann::json IdCache::Get(const std::string &id, const std::string &path, uint32_t blockHeight) const {
			if (!Initialized()) {
				return nlohmann::json();
			}

			std::string value;
			leveldb::Status s = _db->Get(leveldb::ReadOptions(), id, &value);
			if (!s.ok()) {
				Log::getLogger()->error("IDCache get id={} error: {}", id, s.ToString());
				return nlohmann::json();
			}

			nlohmann::json idJson = nlohmann::json::parse(value);
			if (idJson.find(path) == idJson.end()) {
				Log::getLogger()->error("IDCache get id={}, path={} error: not found", id, path);
				return nlohmann::json();
			}

			nlohmann::json pathJson = idJson[path];

			std::ostringstream oss;
			oss << blockHeight;

			if (pathJson.find(oss.str()) == pathJson.end()) {
				Log::getLogger()->error("IDCache get id={}, path={}, blockHeight={} error: not found", id, path, blockHeight);
				return nlohmann::json();
			}

			return pathJson[oss.str()];
		}

		bool IdCache::Delete(const std::string &id) {
			if (!Initialized()) {
				return false;
			}

			leveldb::Status s = _db->Delete(leveldb::WriteOptions(), id);
			if (!s.ok() && !s.IsNotFound()) {
				Log::getLogger()->error("IDCache delete id={} error: {}", id, s.ToString());
				return false;
			}

			return true;
		}

		bool IdCache::Delete(const std::string &id, const std::string &path) {
			if (!Initialized()) {
				return false;
			}

			nlohmann::json idJson;
			std::string value;
			leveldb::Status s = _db->Get(leveldb::ReadOptions(), id, &value);
			if (!s.ok()) {
				if (s.IsNotFound()) {
					return true;
				}
				Log::getLogger()->error("IDCache get id={} error: {}", id, s.ToString());
				return false;
			}

			idJson = nlohmann::json::parse(value);
			if (idJson.empty()) {
				Log::getLogger()->error("id={}, value={} error", id, value);
				return false;
			}

			idJson.erase(path);

			if (idJson.empty()) {
				s = _db->Delete(leveldb::WriteOptions(), id);
				if (!s.ok()) {
					Log::getLogger()->error("IDCache delete id={} error: {}", id, s.ToString());
					return false;
				}
			} else {

				Log::getLogger()->error("ElastosID IdCache::Delete _db->Put id{}  idJson{}", id, idJson.dump());
				s = _db->Put(leveldb::WriteOptions(), id, idJson.dump());
				if (!s.ok()) {
					Log::getLogger()->error("IDCache put id={} error: {}", id, s.ToString());
					return false;
				}
			}

			return true;
		}

		bool IdCache::Delete(const std::string &id, const std::string &path, uint32_t blockHeight) {
			if (!Initialized()) {
				return false;
			}

			nlohmann::json idJson;
			std::string value;
			leveldb::Status s = _db->Get(leveldb::ReadOptions(), id, &value);
			if (!s.ok()) {
				if (s.IsNotFound()) {
					return true;
				}
				Log::getLogger()->error("IDCache get id={} error: {}", id, s.ToString());
				return false;
			}

			idJson = nlohmann::json::parse(value);
			if (idJson.find(path) != idJson.end()) {
				nlohmann::json pathJson = idJson[path];

				if (pathJson.empty()) {
					Log::getLogger()->error("id = {}, path = {} not found", id, path);
					return true;
				}

				std::ostringstream oss;
				oss << blockHeight;

				pathJson.erase(oss.str());

				if (pathJson.empty()) {
					idJson.erase(path);
				} else {
					idJson[path] = pathJson;
				}
			}

			if (idJson.empty()) {
				s = _db->Delete(leveldb::WriteOptions(), id);
				if (!s.ok()) {
					Log::getLogger()->error("IDCache delete id={} error: {}", id, s.ToString());
					return false;
				}
			} else {
				s = _db->Put(leveldb::WriteOptions(), id, idJson.dump());
				if (!s.ok()) {
					Log::getLogger()->error("IDCache put id={} error: {}", id, s.ToString());
					return false;
				}
			}

			return true;
		}

		bool IdCache::Initialized() const {
			if (_db == nullptr) {
				Log::getLogger()->error("IdCache not Initialized ");
				return false;
			}

			return true;
		}

		bool IdCache::DeleteAll() {
			bool ok = true;

			leveldb::Iterator *it = _db->NewIterator(leveldb::ReadOptions());
			for (it->SeekToFirst(); it->Valid(); it->Next()) {
				leveldb::Status s = _db->Delete(leveldb::WriteOptions(), it->key());
				if (!s.ok()) {
					Log::getLogger()->error("delete id={} error: {}", it->key().ToString(), s.ToString());
					ok = false;
					break;
				}
			}
			delete it;

			return ok;
		}

	}
}