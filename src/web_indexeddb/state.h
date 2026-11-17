#ifndef RUYIJS_INDEXED_DATABASE_STATE_H
#define RUYIJS_INDEXED_DATABASE_STATE_H
#include "web_indexeddb/model.h"
#include "runtime/web_kit.h"
#include "browser_environment/environment.h"
#include "interaction/realm.h"
#include "jsapi.h"
namespace ruyijs::web_indexeddb {
struct State;
struct Job {
  State* owner;
  JS::PersistentRootedObject request;
  Key key;
  uint64_t version;
  bool remove, notified = false, blocked = false, done = false;
  int kind=0; // open/delete, get, put, transaction completion.
  std::u16string recordKey;
  Query query;
  std::vector<uint8_t> bytes;
  // Index queries, writes and cursor steps (docs/owners/INDEXED_DB.md, 2026-10-21).
  std::u16string index;
  bool hasIndex = false, noOverwrite = false, all = false;
  IndexKeys indexKeys;
  // Key generator writes: `generate` takes the store's next number when the
  // request runs (injected at `injectPath` for in-line keys, which then
  // recomputes the index keys of `storeName`); `observe` raises the generator
  // past an explicit number key.
  bool generate = false, observe = false;
  // The catalog transaction of the request's object store (EnqueueJob).
  uint32_t txId = 0;
  std::u16string injectPath, storeName;
  CursorStep step;
  JS::PersistentRootedObject cursor;
  // An open request whose upgradeneeded returned with requests still queued in
  // its versionchange transaction: complete/success wait for them (Pump).
  bool upgrading = false;
  uint32_t connection = 0;
  Job(State* s, JSContext* cx, JS::HandleObject r, Key k, uint64_t v, bool d)
      : owner(s), request(cx,r), key(std::move(k)), version(v), remove(d), cursor(cx) {}
};
struct Connection {
  State* owner;
  JS::PersistentRootedObject object;
  Key key;
  uint32_t id;
  Connection(State* s, JSContext* cx, JS::HandleObject o, Key k, uint32_t i)
      : owner(s), object(cx,o), key(std::move(k)), id(i) {}
};
struct Store {
  Model model;
  State* anchor = nullptr;
  bool (*post)(void*, JS::HandleObject) = nullptr;
  void* context = nullptr;
  bool scheduled = false;
  std::vector<std::unique_ptr<Job>> jobs;
  std::vector<std::unique_ptr<Connection>> connections;
};
struct State {
  JSContext* cx;
  WebKit kit;
  interaction::Realm& events;
  browser_environment::Environment& environment;
  State* parent;
  std::shared_ptr<Store> store;
  WebFace factoryFace, requestFace, openFace, databaseFace, transactionFace, storeFace, keyRangeFace, recordFace,
      indexFace, cursorFace, cursorValueFace;
  JS::PersistentRootedObject factory, pump;
  JS::PersistentRootedValue eventCtor, exceptionCtor, addListener, removeListener;
  State(JSContext* c, JS::HandleObject p, PageHelpers& h, interaction::Realm& e,
        browser_environment::Environment& b, State* prior)
      : cx(c), kit(c,p,h,this), events(e), environment(b), parent(prior),
        store(prior ? prior->store : std::make_shared<Store>()),
        factoryFace(c), requestFace(c), openFace(c), databaseFace(c), transactionFace(c), storeFace(c), keyRangeFace(c), recordFace(c),
        indexFace(c), cursorFace(c), cursorValueFace(c), factory(c), pump(c),
        eventCtor(c), exceptionCtor(c), addListener(c), removeListener(c) {}
};
extern const JSClass kFactoryClass, kRequestClass, kDatabaseClass;
extern const JSClass kTransactionClass,kStoreClass,kIndexClass,kCursorClass,kCursorValueClass;
enum { OwnerSlot, ResultSlot, ErrorSlot, ReadySlot, HandlerSlot };
enum { SourceSlot=8, TransactionSlot=9 };
enum { DbOwner, DbId, DbName, DbVersion, DbHandler };
// Slot 8: inside the upgrade; 9: close pending; 10: this connection's versionchange
// transaction (null until an upgrade runs). Its upgrade-created stores keep a null
// StoreTransaction (the upgrade marker) and report this transaction.
enum { DbUpgrade = 10, DbSlots = 11 };
// TxActive: 0 active, 1 finished, 2 aborted, 3 committing (commit(): no new requests).
// TxStores: the per-transaction IDBObjectStore cache (also objectStoreNames' accessor
// id); TxDurability is an accessor id only.
// TxStore: the sorted scope names (a data transaction) or null (versionchange);
// TxIds: the catalog transaction of each scope store, in TxStore order.
enum { TxOwner,TxId,TxDatabase,TxMode,TxHandler,TxActive=7,TxStore=8,TxError=9,TxStores=10,TxIds=11,TxSlots=12,
       TxDurability=TxSlots };
// StoreTxId: the catalog transaction of this store in its IDBTransaction (0: upgrade).
enum { StoreOwner,StoreTransaction,StoreName,StoreKeyPath,StoreDatabase,StoreIndexes,StoreTxId,StoreSlots };
enum { IndexOwner,IndexStore,IndexName,IndexKeyPath,IndexSlots };
// Cursor slots: native position, source, request, direction, key, primaryKey, value.
enum { CursorOwner,CursorNative,CursorSource,CursorRequest,CursorDirection,CursorKey,CursorPrimary,CursorValue,CursorSlots };
bool FactoryGet(JSContext*, unsigned, JS::Value*);
bool FactoryMethod(JSContext*, unsigned, JS::Value*);
bool RequestGet(JSContext*, unsigned, JS::Value*);
bool DatabaseGet(JSContext*, unsigned, JS::Value*);
bool Close(JSContext*, unsigned, JS::Value*);
bool DatabaseMethod(JSContext*,unsigned,JS::Value*);
bool StoreNames(JSContext*,unsigned,JS::Value*);
bool TransactionGet(JSContext*,unsigned,JS::Value*);
bool TransactionMethod(JSContext*,unsigned,JS::Value*);
// The upgrade's IDBTransaction (mode "versionchange", TxId 0: no data transaction).
JSObject* NewVersionTransaction(State&,JS::HandleObject db);
// Ends every catalog transaction of an IDBTransaction's scope together.
bool EndTransaction(State&,JS::HandleObject tx,bool commit);
// The transaction's one IDBObjectStore for `name` (created: an upgrade store to register).
JSObject* TransactionStore(State&,JS::HandleObject tx,const std::u16string& name,const std::u16string& path,
                           JS::HandleObject db,JS::HandleObject created);
bool StoreGet(JSContext*,unsigned,JS::Value*);
bool StoreMethod(JSContext*,unsigned,JS::Value*);
bool InstallRecords(State&);
// Indexes and cursors (index_keys.cpp, index_object.cpp, store_members.cpp, cursor*.cpp).
bool ValidKeyPath(JSContext*,JS::HandleValue,IndexInfo*,bool* valid);
bool KeyPathValue(State&,const IndexInfo&,JS::MutableHandleValue);
bool IndexKeysOf(State&,const IndexInfo&,JS::HandleValue clone,std::vector<std::u16string>*);
bool RecordIndexKeys(State&,const Key&,const std::u16string& store,JS::HandleValue clone,IndexKeys*);
bool StoreConnection(State&,JS::HandleObject store,Key*,JS::MutableHandleObject tx);
JSObject* IndexObject(State&,JS::HandleObject store,const std::u16string& name);
bool IndexGet(JSContext*,unsigned,JS::Value*);
bool IndexSetName(JSContext*,unsigned,JS::Value*);
bool IndexMethod(JSContext*,unsigned,JS::Value*);
bool StoreMember(JSContext*,unsigned,JS::Value*);
bool StoreSetName(JSContext*,unsigned,JS::Value*);
bool EnqueueJob(State&,std::unique_ptr<Job>,JS::HandleObject tx);
// The IDBObjectStore a request source (store, index or cursor) reads.
JSObject* SourceStore(JSObject* source);
// An upgrade whose upgradeneeded has returned: once no request of its
// versionchange transaction is queued, it completes (or aborts) and the open
// request succeeds (or fails). Pump calls it again until then.
bool FinishUpgrade(Job&);
bool UpgradePending(const Store&,const Job&);
bool OpenCursor(State&,JS::HandleObject source,JS::HandleObject tx,const std::u16string* index,JS::HandleValue range,
                JS::HandleValue direction,bool keyOnly,JS::MutableHandleValue request);
bool CursorJob(Job&,const char** error,JS::MutableHandleValue result);
bool CursorGet(JSContext*,unsigned,JS::Value*);
bool CursorMethod(JSContext*,unsigned,JS::Value*);
bool InstallIndexes(State&);
Connection* FindConnection(State&,JS::HandleObject);
JSObject* NewTransaction(State&,JS::HandleObject,const std::vector<std::u16string>& scope,bool);
JSObject* NewObjectStore(State&,JS::HandleObject,const std::u16string&,const std::u16string&,JS::HandleObject db);
JSObject* NewRequest(State&,JS::HandleObject,JS::HandleObject);
bool DataJob(Job&);
bool FinishClose(State&,JS::HandleObject);
bool CloneWrite(State&,JS::HandleValue,std::vector<uint8_t>*);
bool CloneRead(State&,const std::vector<uint8_t>&,JS::MutableHandleValue);
bool HandlerGet(JSContext*, unsigned, JS::Value*);
bool HandlerSet(JSContext*, unsigned, JS::Value*);
bool Pump(JSContext*, unsigned, JS::Value*);
bool Schedule(Store&);
bool Adopt(State&);
bool Complete(Job&);
bool VersionEvent(State&, JS::HandleObject, const char*, uint64_t, uint64_t, bool);
bool ErrorValue(State&, const char*, JS::MutableHandleValue);
bool Throw(State&, const char*);
bool Origin(State&, std::string*);
bool Text(JSContext*, JS::HandleValue, std::u16string*);
bool String(JSContext*, const std::u16string&, JS::MutableHandleValue);
State* Receiver(JSContext*, const JS::CallArgs&, const JSClass*, JS::MutableHandleObject, const char* iface = nullptr);
}
#endif
