// Engine-independent bridge to the unique Document Model domain owners.
// Domain owner: the migrated domain/document_model::DocumentAggregate.
// Model composes document owners; it holds no parallel tree, text, or attributes.
// Native adapter: realm factories, WebIDL conversions, roots, brands, Interaction.
#ifndef RUYIJS_DOCUMENT_MODEL_H
#define RUYIJS_DOCUMENT_MODEL_H
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "document_model/svg_points_types.h"

namespace ruyijs::document_model {
namespace html { struct Session; struct TemplateSink; }
struct FormField;
struct SvgViewBoxValue { float fields[4] = {0, 0, 0, 0}; bool present = false; };  // present: a valid, non-none viewBox
using DomString = std::u16string;
using DomStringView = std::u16string_view;
using DocumentId = uint64_t;
using DraftId = uint64_t;

// Immutable identity: document is the ISSUING namespace, not ownerDocument.
// Neither component is recycled. Only {0,0} is null. Never key by a Node*.
// Future adoption changes ownerDocument and membership, never this wrapper key.
struct NodeKey {
  DocumentId document = 0;
  uint64_t node = 0;
};
enum class NodeKind : uint16_t { Element = 1, Text = 3, ProcessingInstruction = 7,
                                 Comment = 8, Document = 9, DocumentType = 10,
                                 DocumentFragment = 11 };
enum class DocumentMode { Html, Xml };
enum class DocumentInit { MainHtml, EmptyXml };
enum class ParseKind { Html, Xml, TextXml, Svg, Xhtml, PublicHtml };
enum class ReadyState { Loading, Interactive, Complete };
enum class DocumentMetadataField { Domain, Direction, DesignMode };
enum class TokenAction { Contains, Add, Remove, Toggle, Replace };
enum class ErrorCode {
  None, OutOfMemory, NotFound, HierarchyRequest, InvalidCharacter, IndexSize,
  NotSupported, Namespace, NoModification, InvalidState, LimitExceeded, Syntax
};
struct Status {
  ErrorCode code = ErrorCode::None;
  const char* message = "";  // Static literal, including exception/OOM paths.
};
struct Limits {  // Product chooses values explicitly; counts include drafts.
  size_t documents;
  size_t nodes;  // Total across owners, including detached/unwrapped nodes.
  size_t text_units;  // UTF-16 code units, never UTF-8 byte length.
};
struct Draft {
  DraftId token = 0;
  NodeKey root;
};
struct CreateSpec {
  NodeKind kind;
  DomStringView name;  // Element name; validation/folding belongs to domain.
  DomStringView data;  // Text/Comment data; NUL and lone surrogates preserved.
  bool doctype_cloneable = false;  // Public hidden ID state; both ID getters remain empty.
};
// name is the qualified name; namespace_uri/prefix/local_name carry the
// namespace identity the Attr wrapper and the NS attribute family expose
// (empty namespace_uri means the null namespace).
struct AttributeSnapshot { DomString name, value, namespace_uri, prefix, local_name; };
struct ShadowOptions {
  bool closed = false, delegates_focus = false, clonable = false;
  bool serializable = false, manual_slots = false;
};
// An unpublished native transfer. Every identity remains owned by the source
// until the adapter commits the original wrappers; drafts roll back on failure.
struct AdoptionDraft {
  NodeKey root;
  std::vector<Draft> drafts;
  std::vector<std::pair<NodeKey, NodeKey>> identities;
};
struct AdoptionPlacement {
  NodeKey parent, root, replaced;
  std::vector<uint64_t> children, added;
  bool fragment = false;
};
// One prepared original-tree edit; never retained as a second child index.
struct ChildListChange {
  NodeKey parent, node, source_parent, previous, next;
  std::vector<uint64_t> children, added, removed;
  bool fragment = false, replace = false;
};
struct ShadowSnapshot { NodeKey host; ShadowOptions options; };
Status PublicAttributeName(DomStringView, AttributeSnapshot*) noexcept;
struct NodeSnapshot {
  NodeKey key, owner_document;  // owner_document is null for Document nodes.
  NodeKind kind = NodeKind::Document;
  DocumentMode document_mode = DocumentMode::Xml;
  bool is_html_element = false;
  const char* interface_name = "Node";  // Static domain factory classification.
  bool is_connected = false;
  NodeKey parent, parent_element, first_child, last_child, previous_sibling, next_sibling;
  NodeKey root, composed_root, shadow_host, shadow_root;
  size_t child_count = 0;
  DomString node_name;
  std::optional<DomString> local_name, prefix, namespace_uri, node_value;
  size_t data_length = 0;  // CharacterData length in UTF-16 code units.
  bool cloneable = true;
  DomString public_id, system_id;  // DocumentType values, supplied by the original parser.
};
struct DocumentSnapshot {
  NodeKey key, document_element, head, body, active_element, current_script;
  DomString content_type;
  std::optional<DomString> url;  // nullopt reads the external Environment URL port.
  DocumentMode mode = DocumentMode::Xml;
  ReadyState ready_state = ReadyState::Complete;
  bool hidden = true;
  bool xml_parsed = false;  // built by the XML parser: XMLDocument, no-quirks
};
enum class QueryKind { Id, TagName, ClassNames };
enum class TableList { Rows, Bodies, Cells };
struct TableInsertion { NodeKey parent, before; bool body = false; };
// Variadic insertion behind append/prepend/before/after/replaceWith and the
// child-node remove(). A null item node creates a Text node from the item
// text in the target's document; node and text are mutually exclusive.
enum class InsertMode : uint8_t { Append, Prepend, Before, After, ReplaceWith, Detach };
struct Item { NodeKey node; DomString text; };
// One real change in the unique tree, produced where the change happens.
// web_observers turns these into MutationRecord objects; this bridge never
// decides which observer sees which change. The sibling fields are the added
// node's neighbours after an insert, or the removed node's neighbours from
// before it left the tree, and are null when there is none.
enum class MutationKind : uint8_t { ChildList, Attributes, CharacterData };
struct MutationNotice {
  uint64_t sequence = 0;
  MutationKind kind = MutationKind::ChildList;
  NodeKey target;                        // Parent, element or CharacterData.
  std::vector<NodeKey> added, removed;   // childList, in tree order.
  NodeKey previous_sibling, next_sibling;
  DomString attribute_name;              // Attributes local name.
  DomString attribute_namespace;         // Attributes namespace URI, "" none.
  bool has_old_value = false;            // Attributes / CharacterData.
  DomString old_value;                   // Value before the change.
};


// All DTOs are short-lived snapshots. Native never retains business-state copies.
// All methods catch C++ exceptions internally; no exception crosses into JSAPI.
// Dynamic out values are replaced only on success. Status creation allocates nothing.
// Inputs are views, valid only for this call; persistent strings live in the owner.
// Mutations validate/reserve first; errors leave all published trees/data unchanged.
class Model final {
 public:
  Model() noexcept = default;
  ~Model() noexcept;
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;
  Status Initialize(const Limits& limits) noexcept;
  Status ReadDocumentMetadata(NodeKey, DocumentMetadataField, DomStringView url, DomString*) const noexcept;
  Status WriteDocumentMetadata(NodeKey, DocumentMetadataField, DomStringView) noexcept;

  // MainHtml builds one HTML document with html/head/body via aggregate operations.
  // EmptyXml implements new Document(): empty XML semantics, complete readyState.
  // Native uses HTMLDocument.prototype for MainHtml, Document.prototype for EmptyXml.
  // Neither path parses markup or starts navigation. URL/defaultView are explicit
  // application/environment ports; this bridge creates no second Location state.
  Status CreateDocument(DocumentInit init, Draft* result) noexcept;
  Status CreatePublicDocument(DomStringView namespace_uri, DomStringView name, NodeKey doctype,
      Draft*, std::vector<std::pair<NodeKey, NodeKey>>* rekey) noexcept;
  // DOMParser: all public MIME types run the existing inert HTML tokenizer/tree
  // builder. XML types retain Document mode/prototype and their original MIME.
  // `location` is the creating document's URL, which an XML parsererror
  // document names (Firefox's "Location:" line).
  Status ParseDocument(ParseKind kind, DomStringView markup, Draft* result,
                       DomStringView location = {}) noexcept;
  Status Create(NodeKey document, const CreateSpec& spec, Draft* result) noexcept;
  // DocumentFragment is an insertion container, never a tree position: it
  // owns children only until they are pre-inserted elsewhere.
  Status CreateFragment(NodeKey document, Draft* result) noexcept;
  Status AttachShadow(NodeKey host, const ShadowOptions&, Draft*) noexcept;
  Status InspectShadow(NodeKey root, ShadowSnapshot*) const noexcept;
  Status ShadowScope(NodeKey node, NodeKey*) const noexcept;
  Status TemplateContent(NodeKey element, NodeKey* fragment) const noexcept;
  Status TemplateDocument(NodeKey document, NodeKey* result) noexcept;
  Status BindTemplateContent(NodeKey element, NodeKey fragment) noexcept;
  Status DocumentLinks(std::vector<std::pair<NodeKey, NodeKey>>* links) const noexcept;
  void RekeyDocumentReferences(const std::vector<std::pair<NodeKey, NodeKey>>& pairs) noexcept;
  Status ParsedDocumentType(NodeKey document, NodeKey*) const noexcept;
  Status ParsedDocumentTypeOwners(NodeKey node, std::vector<NodeKey>*) const noexcept;
  // importNode clones a node of another document into this one; the source is
  // untouched and the copy's ownerDocument is this document.
  Status ImportNode(NodeKey document, NodeKey source, bool deep, Draft* result) noexcept;
  Status ImportNode(NodeKey document, const Model& source_model, NodeKey source,
                    bool deep, Draft* result) noexcept;
  // adoptNode moves a subtree between documents. The domain has one id space per
  // document, so the move is a structural copy into the target plus removal from
  // the source; `rekey` carries the old->new identity pairs so the realm can
  // repoint its wrappers, keeping the JS object identity the caller observes.
  Status AdoptNode(NodeKey document, NodeKey source, Draft* result,
                   std::vector<std::pair<NodeKey, NodeKey>>* rekey) noexcept;
  Status PrepareAdoption(NodeKey document, const Model& source_model, NodeKey source,
                         AdoptionDraft*, bool fragment_insertion = false) noexcept;
  Status PrepareAdoptionPlacement(NodeKey parent, NodeKey root, NodeKey before,
                                  NodeKey replaced, AdoptionPlacement*) noexcept;
  Status CommitAdoptionPlacement(AdoptionPlacement&) noexcept;
  Status CheckChildInsertion(NodeKey parent, const Model& source, NodeKey node) const noexcept;
  Status PrepareChildListChange(NodeKey parent, NodeKey node, bool replace, ChildListChange*) noexcept;
  Status CommitChildListChange(ChildListChange&) noexcept;
  Status DetachTransferred(NodeKey source, const std::vector<std::pair<NodeKey, NodeKey>>& identities,
                            bool fragment) noexcept;
  bool SetIdentityTransfer(bool pending) noexcept;
  Status Clone(NodeKey source, bool deep, Draft* result,
               std::optional<DomStringView> source_document_url = std::nullopt) noexcept;

  // Create/Clone prepare an unpublished root/subtree. Inspect accepts its keys.
  // Native prepares wrapper/root/cache, then Interaction.Attach, then Publish.
  // Publish consumes a valid live draft with NO allocation, callback, or failure.
  // After Attach there must be no fallible step. Rollback removes only unpublished
  // records; published/detached nodes survive. Rollback of a consumed token is a no-op.
  // Clone(Document) creates a new document owner with the same document mode;
  // external URL sources require a call-time snapshot; owned URLs copy directly.
  // ordinary clones retain the source's current ownerDocument, with fresh identities.
  void Publish(DraftId draft) noexcept;
  void Rollback(DraftId draft) noexcept;

  // Monotonic owner revision, bumped by every tree or attribute mutation.
  // Native-side caches (live collections) compare it instead of polling the
  // tree; it never reaches script as a value of its own.
  uint64_t Revision() const noexcept;
  // The revision of everything live collections and token views read: every
  // mutation except a change of the style attribute (which only styles and
  // layout read) and an attribute write that leaves the value unchanged, so
  // those writes do not re-collect them.
  uint64_t StructureRevision() const noexcept;
  // The revision of the tree shape and character data: unchanged by attribute
  // writes, so a layout can tell an attribute-only change from a tree change.
  uint64_t TreeRevision() const noexcept;
  // Real-change log for the observer owner (src/web_observers). Recording is
  // off until an observer registers, so script that never observes pays one
  // flag test per mutation. MutationsSince copies every notice after a
  // sequence cursor, so a consumer never replays changes made before it began
  // observing. The log is bounded and DroppedMutations counts what a lagging
  // consumer lost. The sink carries no JS value at all: it only tells the
  // consumer that it should queue its own microtask.
  void SetMutationRecording(bool enabled) noexcept;
  bool MutationRecording() const noexcept;
  bool IdentityTransferPending() const noexcept;
  uint64_t MutationSequence() const noexcept;
  uint64_t DroppedMutations() const noexcept;
  Status MutationsSince(uint64_t sequence, std::vector<MutationNotice>* result) const noexcept;
  void SetMutationSink(void (*sink)(void*), void* context) noexcept;
  // Structure-change port for the runtime frame lifecycle: every successful
  // insert, replacement or removal of the unique tree reports its document and
  // the subtrees it inserted. The observer log above stays the only record
  // list; this port keeps no state and hands out no JS value.
  void SetStructureSink(void (*sink)(void*, DocumentId, const std::vector<NodeKey>&),
                        void* context) noexcept;
  // Wrapper-lifetime port: the same change point as the structure sink, with
  // both the keys that entered and the keys that left the tree. The document
  // model binds it to root a wrapper whose node entered the tree and to release
  // the root of one whose node left it (T41).
  void SetTreeSink(void (*sink)(void*, DocumentId, const std::vector<NodeKey>&,
                                const std::vector<NodeKey>&),
                   void* context) noexcept;
  // Collection request for the node cap (T41): the realm runs one engine
  // collection and reclaims the wrappers it dropped, so the cap only ever
  // bounds live nodes. Called at most once per cap check.
  void SetReclaimSink(void (*sink)(void*), void* context) noexcept;
  // Attribute-change port for the runtime frame owner: every committed
  // attribute write of the unique tree reports the element key and the folded
  // local name, recording or not. The frame owner reads the new value back
  // through GetAttribute, so this port carries no JS value and keeps no state.
  // The namespace argument is empty for no namespace; `old` is the previous
  // value (null when the attribute was absent).
  void SetAttributeSink(void (*sink)(void*, DocumentId, NodeKey, DomStringView, DomStringView,
                                     const DomString* old),
                        void* context) noexcept;
  // Styling receives committed changes even when observer recording is off.
  void SetStyleSink(void (*sink)(void*, NodeKey, MutationKind, DomStringView,
                                 DomStringView, const DomString*), void* context) noexcept;


  Status Inspect(NodeKey node, NodeSnapshot* result) const noexcept;
  Status CreateElementNS(NodeKey document, DomStringView namespace_uri,
                        DomStringView qualified_name, const std::optional<DomString>& is,
                        Draft* result) noexcept;
  Status InspectDocument(NodeKey document, DocumentSnapshot* result) const noexcept;
  // document.title: derived from the head's first title element and written back
  // to it, so the tree stays the only source. XML documents keep the empty string.
  Status Title(NodeKey document, DomString* result) const noexcept;
  Status DialogReturnValue(NodeKey key, DomString* result) const noexcept;
  Status EmbeddedInteger(NodeKey key, DomStringView name, double* out) const noexcept;
  Status SetEmbeddedInteger(NodeKey key, DomStringView name, int64_t value) noexcept;
  Status ImageAttribute(NodeKey key, DomStringView name, std::optional<DomString>* out) const noexcept;
  Status ImageSpacing(NodeKey key, DomStringView name, uint32_t* out) const noexcept;
  Status ElementViewDimension(NodeKey, bool height, int32_t* out) const noexcept;
  Status SetImageSpacing(NodeKey key, DomStringView name, uint32_t value) noexcept;
  Status SetDialogReturnValue(NodeKey key, DomStringView value) noexcept;
  Status DialogAction(NodeKey key, bool close, const std::optional<DomString>& value) noexcept;
  Status SetTitle(NodeKey document, DomStringView value) noexcept;
  // The document's focused element (a null node clears it); FocusedElement
  // reports a null node when nothing in the tree is focused.
  Status SetFocusedElement(NodeKey document, NodeKey element) noexcept;
  Status FocusedElement(NodeKey document, NodeKey* element) const noexcept;
  // Live-collection sources: document descendants that are a/area elements with an
  // href attribute, and a elements with a name attribute.
  Status Links(NodeKey scope, std::vector<NodeKey>* result) const noexcept;
  Status Anchors(NodeKey scope, std::vector<NodeKey>* result) const noexcept;
  Status TableNodes(NodeKey scope, TableList, std::vector<NodeKey>*) const noexcept;
  Status TableIndex(NodeKey node, unsigned kind, int32_t*) const noexcept;
  Status TableInsertPlan(NodeKey scope, int32_t index, TableInsertion*) const noexcept;
  Status TableDeleteTarget(NodeKey scope, int32_t index, NodeKey*) const noexcept;
  Status TablePart(NodeKey scope, DomStringView tag, NodeKey* part, NodeKey* before) const noexcept;
  Status TableAttribute(NodeKey node, DomStringView attr, DomString*) const noexcept;
  Status TableSpan(NodeKey node, DomStringView attr, uint32_t*) const noexcept;
  Status Children(NodeKey node, std::vector<NodeKey>* result) const noexcept;
  // Tag, kind, owner document and connection, without the sibling scan or the
  // name strings Inspect copies. Custom-element upgrade reads only these.
  Status PeekElement(NodeKey node, NodeSnapshot* result) const noexcept;
  Status AncestorPath(NodeKey node, std::vector<NodeKey>* result) const noexcept;
  // Event topology flags bit 0 marks a closed root; one spare flag is reserved for Window.
  Status EventPath(NodeKey node, bool composed, std::vector<NodeKey>* result,
                   std::vector<uint8_t>* flags) const noexcept;
  Status Retarget(NodeKey node, const Model* againstModel, NodeKey against,
                  NodeKey* result, bool* inShadowTree) const noexcept;
  Status Contains(NodeKey node, NodeKey other, bool* result) const noexcept;
  Status IsEqual(NodeKey node, NodeKey other, bool* result) const noexcept;
  // AncestorPath returns [self,parent,...,root]; Interaction consumes a snapshot,
  // never a cached parent tree. Null other means false for Contains/IsEqual.

  // before={0,0} means append; Insert(parent,node,node) preserves its position.
  // Validate parent kind, reference membership, cycles and single-document-root
  // constraints BEFORE detaching. Replace is one transaction, not remove+insert.
  // Remove checks the actual parent and only detaches; it never destroys a node.
  // A node whose owner document is not the parent's is adopted into the
  // parent's document first, like Firefox, instead of being rejected; the
  // caller passes a fresh `rekey` vector and repoints its realm caches with it.
  Status Insert(NodeKey parent, NodeKey child, NodeKey before = {},
                std::vector<std::pair<NodeKey, NodeKey>>* rekey = nullptr) noexcept;
  Status Replace(NodeKey parent, NodeKey child, NodeKey old_child,
                 std::vector<std::pair<NodeKey, NodeKey>>* rekey = nullptr) noexcept;
  Status Remove(NodeKey parent, NodeKey child) noexcept;
  // T41 owner teardown: erases one node and its whole subtree from its owning
  // document. The caller has already established that neither the tree (the node
  // is not connected) nor a live wrapper reaches the subtree. Removing the
  // nodes bumps the revision, so every live collection re-collects.
  Status Release(NodeKey node) noexcept;
  // Every key of one ownership subtree (including shadow roots), root first; the membership and
  // reclamation walks need one read of the unique tree, not a second one.
  Status Subtree(NodeKey root, std::vector<NodeKey>* result) const noexcept;
  // Topmost ownership ancestor of one node, crossing ShadowRoot -> host, and
  // whether the owning Document holds that node (its root is the Document
  // node). T41 uses this to name the detached trees a live wrapper can still
  // reach through parentNode/host: a Document and everything it holds are never
  // released, and one detached tree is one release decision. No snapshot
  // strings are built, because the walks read this once per node.
  Status Root(NodeKey node, NodeKey* result, bool* connected) const noexcept;

  Status ReadSvgViewBox(NodeKey, bool* supported, SvgViewBoxValue*) const noexcept;
  Status WriteSvgViewBoxField(NodeKey, unsigned field, float value) noexcept;
  // Live polygon/polyline points (model_svg_points.cpp) and SVG length
  // attributes (model_svg_length.cpp, field = kSvgLengthFields index).
  Status ReadSvgPoints(NodeKey, SvgPointsSnapshot*) const noexcept;
  Status MutateSvgPoints(NodeKey, unsigned operation, uint32_t index, const SvgPointRef& input,
                         SvgPointRef* result) noexcept;
  Status WriteSvgPointField(NodeKey, const SvgPointRef& point, unsigned field, float value) noexcept;
  Status ReadSvgLength(NodeKey, unsigned field, bool* supported, SvgLengthValue*) const noexcept;
  Status WriteSvgLength(NodeKey, unsigned field, const SvgLengthValue&) noexcept;
  Status GetAttribute(NodeKey element, DomStringView name,
                      std::optional<DomString>* result) const noexcept;
  Status SetAttribute(NodeKey element, DomStringView name, DomStringView value) noexcept;
  Status RemoveAttribute(NodeKey element, DomStringView name) noexcept;
  Status Attributes(NodeKey element, std::vector<AttributeSnapshot>* result) const noexcept;

  // NS attribute family. Namespace identity is stored with the attribute value
  // in the same domain Element, so getAttributeNS and getAttribute(qualified)
  // read one source. A null (empty) namespace is a legal namespace.
  Status GetAttributeNS(NodeKey element, DomStringView namespace_uri, DomStringView local,
                        std::optional<DomString>* result) const noexcept;
  Status SetAttributeNS(NodeKey element, DomStringView namespace_uri, DomStringView qualified,
                        DomStringView value) noexcept;
  Status RemoveAttributeNS(NodeKey element, DomStringView namespace_uri, DomStringView local) noexcept;
  Status HasAttributeNS(NodeKey element, DomStringView namespace_uri, DomStringView local,
                        bool* result) const noexcept;
  // toggleAttribute semantics; has_force distinguishes an absent second argument.
  Status ToggleAttribute(NodeKey element, DomStringView name, bool force, bool has_force,
                         bool* result) noexcept;
  // Missing attribute = nullopt, present empty value = u"". Preserve insertion order.
  // HTML name folding depends on HTML element + HTML owner document; XML preserves case.

  Status SetNodeValue(NodeKey node, DomStringView value) noexcept;
  Status SubstringData(NodeKey node, uint32_t offset, uint32_t count,
                       DomString* result) const noexcept;
  Status ReplaceData(NodeKey node, uint32_t offset, uint32_t count,
                     DomStringView value) noexcept;
  Status TextContent(NodeKey node, std::optional<DomString>* result) const noexcept;
  Status WholeText(NodeKey node, DomString* result) const noexcept;
  Status SplitTextInto(NodeKey source, NodeKey destination, uint32_t offset) noexcept;
  Status ClearFragment(NodeKey node) noexcept;
  Status SetTextContent(NodeKey node, DomStringView value) noexcept;
  // CharacterData indexing/count is UTF-16; out-of-range offset => IndexSize.
  // Document nodeValue/textContent are null and setters have no effect.
  // Element nodeValue is null/no-op; textContent replaces children atomically.
  // New textContent children and clone descendants may be wrapped lazily: wrapper
  // allocation failure preserves existing domain nodes for a later retry.

  Status Query(NodeKey root, QueryKind kind, DomStringView value,
               std::vector<NodeKey>* result) const noexcept;
  // Query visits descendants in tree order, excluding an Element root itself.
  // Id returns at most the first match (empty ID matches none); Document scope
  // excludes detached nodes. TagName supports "*"; ClassNames requires all tokens.
  // This is a snapshot, not a claimed live NodeList/HTMLCollection or CSS selector API.

  Status QuerySelector(NodeKey root, DomStringView selector,
                       std::vector<NodeKey>* result) const noexcept;
  Status AllNamed(NodeKey root, const DomStringView* name,
                  std::vector<NodeKey>* nodes, std::vector<DomString>* names) const noexcept;
  Status WindowNamed(NodeKey root, DomStringView name,
                     std::vector<NodeKey>* nodes) const noexcept;
  // The CSS subset behind querySelector/querySelectorAll (model_selector.cpp).
  // Unsupported or malformed syntax reports Syntax, never a silent non-match.
  // Results are descendants of root in tree order, excluding root itself.
  // matches/closest reuse the same Selector; invalid syntax reports Syntax.
  Status Matches(NodeKey node, DomStringView selector, bool* result) const noexcept;
  Status Closest(NodeKey node, DomStringView selector, NodeKey* result) const noexcept;
  // getElementsByTagNameNS: "*" matches any namespace and any local name; an
  // empty namespace matches elements with no namespace. HTML elements carry the
  // XHTML namespace, every other element carries the null namespace.
  Status ElementsByTagNameNS(NodeKey scope, DomStringView namespace_uri, DomStringView local,
                             std::vector<NodeKey>* result) const noexcept;
  // Merges adjacent CharacterData siblings and removes empty ones in the whole
  // subtree, in tree order.
  Status Normalize(NodeKey node) noexcept;
  // compareDocumentPosition bitmask computed from the owner snapshots.
  Status CompareDocumentPosition(NodeKey node, NodeKey other, uint32_t* result) const noexcept;
  // The MutationObserver subtree test: true when `ancestor` is `node` itself or
  // one of its ancestors, computed from the unique tree and never a second
  // index. A node of another document has no relationship with it.
  Status AncestorOrSelf(NodeKey ancestor, NodeKey node, bool* result) const noexcept;

  // Reference resolution, cycle/document checks, Text creation and every
  // insert happen here, so an item the owner rejects leaves the published
  // tree unchanged. Before/After/ReplaceWith/Detach without a parent are
  // no-ops, matching ChildNode; Append/Prepend always need an insertable
  // parent, so a Document target still accepts one element child.
  Status InsertItems(NodeKey target, InsertMode mode, const std::vector<Item>& items,
                     std::vector<std::pair<NodeKey, NodeKey>>* rekey = nullptr) noexcept;
  Status ChildrenElements(NodeKey node, std::vector<NodeKey>* result) const noexcept;
  Status ElementsByName(NodeKey scope, DomStringView name,
                        std::vector<NodeKey>* result) const noexcept;
  // Live-collection sources: Element children in order, and scope descendants
  // whose name attribute equals name (folded by the element's own mode).

  Status AttributeTokens(NodeKey element, DomStringView name, std::vector<DomString>* result) const noexcept;
  Status OptionSelected(NodeKey, bool* result) const noexcept;
  Status SetOptionSelected(NodeKey, bool value) noexcept;
  Status CustomValidity(NodeKey, DomString* result) const noexcept;
  Status SetCustomValidity(NodeKey, DomStringView value) noexcept;
  Status LabelControl(NodeKey, NodeKey* result) const noexcept;
  Status MeterValue(NodeKey, DomStringView name, double* result) const noexcept;
  Status HeadAttribute(NodeKey, DomStringView name, std::optional<DomString>* result) const noexcept;
  Status ContentAttribute(NodeKey, DomStringView name, DomString* result) const noexcept;
  Status SetContentAttribute(NodeKey, DomStringView name, DomStringView value) noexcept;
  Status ContentInteger(NodeKey, DomStringView name, int32_t* result) const noexcept;
  Status SetContentInteger(NodeKey, DomStringView name, int32_t value) noexcept;
  Status SupportsScriptType(DomStringView type, bool* result) const noexcept;
  Status AnchorPolicy(NodeKey, DomString* result) const noexcept;
  Status SetAnchorPolicy(NodeKey, DomStringView value) noexcept;
  Status SetMeterValue(NodeKey, DomStringView name, double value) noexcept;
  Status InputText(NodeKey, DomStringView name, DomString* result) const noexcept;
  Status SetInputValue(NodeKey, DomStringView value) noexcept;
  Status ResetInputValues(NodeKey form) noexcept;
  Status InputNumber(NodeKey, DomStringView name, double* result) const noexcept;
  Status InputTypeValue(DomStringView value, DomString* result) const noexcept;
  Status InputWillValidate(NodeKey, bool* result) const noexcept;
  Status InputChecked(NodeKey, bool* result, bool indeterminate = false) const noexcept;
  Status SetInputChecked(NodeKey, bool value, bool indeterminate = false) noexcept;
  Status ControlValidation(NodeKey, uint32_t* flags, DomString* message = nullptr) const noexcept;
  Status SetInputNumber(NodeKey, double value, bool step) noexcept;
  Status InputRange(NodeKey, DomStringView replacement, int32_t start, int32_t end, bool explicit_range) noexcept;
  Status TextareaText(NodeKey, DomStringView name, DomString* result) const noexcept;
  Status TextareaNumber(NodeKey, DomStringView name, double* result) const noexcept;
  Status TextareaWillValidate(NodeKey, bool* result) const noexcept;
  Status SetTextareaValue(NodeKey, DomStringView) noexcept;
  Status SetTextareaSelection(NodeKey, uint32_t start, uint32_t end, DomStringView direction, int field) noexcept;
  Status TextareaRange(NodeKey, DomStringView replacement, uint32_t start, uint32_t end,
                       bool explicit_range, DomStringView mode) noexcept;
  Status ButtonAttribute(NodeKey, DomStringView name, DomString* result) const noexcept;
  Status FormNodes(NodeKey form, const DomStringView* name, std::vector<NodeKey>* result) const noexcept;
  Status FormOwner(NodeKey, NodeKey*) const noexcept;
  Status FormControlInfo(NodeKey, bool* disabled, bool* submitButton) const noexcept;
  Status FormFields(NodeKey, NodeKey submitter, std::vector<FormField>*) const noexcept;
  Status FormAttribute(NodeKey, DomStringView name, DomString* result) const noexcept;
  Status SetFormAttribute(NodeKey, DomStringView name, DomStringView value) noexcept;
  Status RadioValue(NodeKey form, DomStringView name, DomString* result) const noexcept;
  Status SetRadioValue(NodeKey form, DomStringView name, DomStringView value) noexcept;
  Status OptionText(NodeKey, DomString* result) const noexcept;
  Status SelectSize(NodeKey, uint32_t* result) const noexcept;
  Status SetSelectSize(NodeKey, uint32_t value) noexcept;
  Status SetSelectAutocomplete(NodeKey, DomStringView value) noexcept;
  Status EditTokens(NodeKey element, DomStringView name, TokenAction action, DomStringView token,
                    DomStringView replacement, bool force, bool has_force, bool* result) noexcept;
  Status EditPartTokens(NodeKey, TokenAction, const std::vector<DomString>&,
                       bool force, bool has_force, bool* result) noexcept;
  // DOMTokenList operations. An empty token or one containing ASCII whitespace
  // reports Syntax, the token list's DOMException SyntaxError; every mutation
  // writes back through SetAttribute, so the shared budget and the class
  // attribute remain one source of truth.

  // HTML markup (simplified tree construction, docs/DEVIATIONS.md D7). HTML
  // elements of HTML documents only; XML fragment parsing is NotSupported.
  // SetInnerHtml parses into an unpublished root first, so a failure leaves the
  // old children; replaced children are detached, never destroyed. Script
  // elements it creates are inert: only the document parser yields scripts.
  Status InnerHtml(NodeKey element, DomString* result) const noexcept;
  Status SetInnerHtml(NodeKey element, DomStringView markup) noexcept;
  // Serialization of one node including its own tag (DomTreeService).
  Status OuterHtml(NodeKey node, DomString* result) const noexcept;
  // Fragment parsing for innerHTML/outerHTML/insertAdjacentHTML: the SAME
  // tokenizer/tree-builder path, into an unpublished context root. The caller
  // moves the returned nodes and then always calls DiscardFragment(root), which
  // destroys the empty root and any node still owned by it on a failure path.
  Status ParseFragment(NodeKey context, DomStringView markup, NodeKey* root,
                       std::vector<NodeKey>* nodes) noexcept;
  void DiscardFragment(NodeKey root) noexcept;
  // Range.createContextualFragment: parses `markup` with the context element
  // (`context`, or a body element when it is null or the HTML document's html
  // element) into `fragment`, a DocumentFragment draft of the same document.
  // Unlike innerHTML, the scripts are unmarked as already started and as
  // parser-inserted, so inserting the fragment runs them.
  Status ParseContextualFragment(NodeKey context, NodeKey fragment, DomStringView markup) noexcept;
  // outerHTML setter / insertAdjacentHTML. ReplaceOuter rejects a parentless
  // node (NoModification); position is 0 beforebegin, 1 afterbegin, 2 beforeend,
  // 3 afterend and the parse context is the parent for before/after positions.
  Status ReplaceOuter(NodeKey node, DomStringView markup) noexcept;
  Status InsertAdjacent(NodeKey node, int32_t position, DomStringView markup) noexcept;
  // Document load. BeginParse detaches the document's children (identity and
  // listeners stay), sets readyState loading and clears currentScript.
  // ResumeParse inserts nodes until a parser-inserted script element closes
  // (*script is that element) or the input ends (*script null, session gone).
  // Nodes appear incrementally, so a script sees the tree parsed before it.
  Status BeginParse(NodeKey document, DomStringView markup) noexcept;
  Status ResumeParse(NodeKey document, NodeKey* script) noexcept;
  // document.write while the parser is paused at the script it just closed:
  // |text| enters the input at the insertion point and is parsed next.
  // *written is false (nothing changes) when there is no insertion point.
  Status ParserWrite(NodeKey document, DomStringView text, bool* written) noexcept;
  // document.open(): unless a script of the document's own parser is running
  // (*opened false, nothing changes), detaches the children, sets readyState
  // loading and starts a script-created parser whose insertion point is the end
  // of its input, so later writes parse at once. CloseParse ends that input
  // (implied elements are created) and sets readyState complete.
  Status OpenParse(NodeKey document, bool* opened) noexcept;
  Status CloseParse(NodeKey document) noexcept;
  Status SetReadyState(NodeKey document, ReadyState state) noexcept;
  // Null clears it; otherwise an HTML script element of that document.
  Status SetCurrentScript(NodeKey document, NodeKey script) noexcept;
  Status PrepareScript(NodeKey, bool parser, bool* execute, bool* external) noexcept;
  Status ScriptAsync(NodeKey, bool* result) const noexcept;
  // The parser-recorded document position of a script's text (0, 0 if none).
  Status ScriptPosition(NodeKey, uint32_t* line, uint32_t* column) const noexcept;
  Status SetScriptAsync(NodeKey, bool value) noexcept;
  void SetScriptSink(void (*sink)(void*, NodeKey, bool subtree), void*) noexcept;

 private:
  struct Impl;  // Composes the sole DocumentAggregate instances and identity allocator.
  Impl& State() const;
  // Connects a parser session's change sink to this owner's notice log.
  static void BindParserNotice(html::Session& session, Impl& impl, uint64_t document);
  // Points a tree builder's template contents at this owner's inert documents.
  static void BindTemplateSink(html::TemplateSink& sink, Impl& impl, uint64_t document);
  // The one cross-document move, shared by Document.adoptNode and the
  // pre-insertion adoption above: copy the subtree into `document`, place the
  // copy at (parent, before|replaced) so the target validates it with the same
  // domain entry points a same-document edit uses, then remove the source and
  // report both sides. A rejected placement deletes the copy and leaves both
  // documents unchanged. Returns the moved node's key in the target document.
  NodeKey MoveIn(NodeKey document, NodeKey source, NodeKey parent, NodeKey before,
                 NodeKey replaced, std::vector<std::pair<NodeKey, NodeKey>>* rekey);
  // A DocumentFragment/ShadowRoot is a temporary container for insertion: its
  // own identity remains in the source document while only its children are
  // copied, placed and rekeyed into the target document.
  std::vector<NodeKey> MoveFragmentIn(
      NodeKey document, NodeKey source, NodeKey parent, NodeKey before,
      NodeKey replaced, std::vector<std::pair<NodeKey, NodeKey>>* rekey);
  // Removes an already-copied source subtree from its old document and reports
  // the departure in that document's own change log. `moved` is the node's key
  // in the target document: the JS object keeps its identity across the realm
  // rekey, so the delivered removal record must name that identity, like
  // Firefox's.
  void CommitTransfer(NodeKey source, NodeKey moved);
  Impl* impl_ = nullptr;
};
bool SupportedEditCommand(DomStringView) noexcept;

// Checkpoint gaps: namespaces/createElementNS, doctype/template, the full HTML5
// tree builder (tables, adoption agency, foreign content) and child realms.
// Cross-document mutation adopts the node into the parent's document, as
// Firefox does; only the same-owner case is supported, because a node from
// another realm's document owner has no identity in this Model.
// Preserve immutable keys so later atomic owner transfer needs no wrapper rekey.
// A transfer may keep lookup indexes, but never a second authoritative node/tree.
// Original 20 GTests keep ASCII adapters onto the same UTF-16 domain state.
// Adapt only illegal multiple-Document-root fixtures to one legal root and HTML
// serialization expectations to lowercase; preserve source baselines + SHA/reasons.
// Source remove_node remains an internal destructive operation, never JS removeChild.
}
#endif
