//
// Created by Perfare on 2020/7/4.
// Modified to generate: dump.cs, script.json, il2cpp.h, and DummyDll per-assembly files
//

#include "il2cpp_dump.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include "xdl.h"
#include "log.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"

#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

static uint64_t il2cpp_base = 0;

void init_il2cpp_api(void *handle) {
#define DO_API(r, n, p) {                      \
    n = (r (*) p)xdl_sym(handle, #n, nullptr); \
    if(!n) {                                   \
        LOGW("api not found %s", #n);          \
    }                                          \
}

#include "il2cpp-api-functions.h"

#undef DO_API
}

// ==================== Utility Functions ====================

static std::string json_escape(const std::string &s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

static std::string sanitize_name(const std::string &name) {
    std::string out;
    out.reserve(name.size());
    for (size_t i = 0; i < name.size(); ++i) {
        char c = name[i];
        if (c == '<' || c == '>' || c == '`' || c == '|' || c == '/' || c == '.' ||
            c == '-' || c == '[' || c == ']' || c == ',' || c == ' ' || c == '=' ||
            c == '(' || c == ')' || c == '{' || c == '}' || c == '+' || c == '!' ||
            c == '@' || c == '#' || c == '$' || c == '%' || c == '^' || c == '&' ||
            c == '*' || c == '?' || c == ':' || c == ';' || c == '~') {
            out += '_';
        } else {
            out += c;
        }
    }
    if (!out.empty() && out[0] >= '0' && out[0] <= '9') {
        out = "_" + out;
    }
    if (out.empty()) {
        out = "_unnamed";
    }
    return out;
}

static std::string get_full_name(Il2CppClass *klass) {
    std::string ns;
    std::string name;
    auto ns_cstr = il2cpp_class_get_namespace(klass);
    auto name_cstr = il2cpp_class_get_name(klass);
    if (ns_cstr) ns = ns_cstr;
    if (name_cstr) name = name_cstr;
    if (ns.empty()) return name;
    return ns + "." + name;
}

static std::string il2cpp_type_to_c(const Il2CppType *type, bool for_field = false) {
    if (!type) return "void*";

    auto typeEnum = type->type;
    switch (typeEnum) {
        case IL2CPP_TYPE_VOID:       return "void";
        case IL2CPP_TYPE_BOOLEAN:    return "bool";
        case IL2CPP_TYPE_CHAR:       return "uint16_t";
        case IL2CPP_TYPE_I1:         return "int8_t";
        case IL2CPP_TYPE_U1:         return "uint8_t";
        case IL2CPP_TYPE_I2:         return "int16_t";
        case IL2CPP_TYPE_U2:         return "uint16_t";
        case IL2CPP_TYPE_I4:         return "int32_t";
        case IL2CPP_TYPE_U4:         return "uint32_t";
        case IL2CPP_TYPE_I8:         return "int64_t";
        case IL2CPP_TYPE_U8:         return "uint64_t";
        case IL2CPP_TYPE_R4:         return "float";
        case IL2CPP_TYPE_R8:         return "double";
        case IL2CPP_TYPE_STRING:     return "Il2CppString*";
        case IL2CPP_TYPE_I:          return "intptr_t";
        case IL2CPP_TYPE_U:          return "uintptr_t";
        case IL2CPP_TYPE_OBJECT:     return "Il2CppObject*";
        case IL2CPP_TYPE_SZARRAY:    return "Il2CppArray*";
        case IL2CPP_TYPE_ARRAY:      return "Il2CppArray*";
        case IL2CPP_TYPE_TYPEDBYREF: return "Il2CppObject*";
        case IL2CPP_TYPE_CLASS:
        case IL2CPP_TYPE_GENERICINST: {
            auto klass = il2cpp_class_from_type(type);
            if (klass) {
                std::string n = sanitize_name(get_full_name(klass));
                if (il2cpp_class_is_valuetype(klass)) {
                    return "struct " + n + "_o";
                }
                return "struct " + n + "_o*";
            }
            return "void*";
        }
        case IL2CPP_TYPE_VALUETYPE: {
            auto klass = il2cpp_class_from_type(type);
            if (klass) {
                std::string n = sanitize_name(get_full_name(klass));
                return "struct " + n + "_o";
            }
            return "void*";
        }
        case IL2CPP_TYPE_PTR:
        case IL2CPP_TYPE_FNPTR:
            return "void*";
        case IL2CPP_TYPE_BYREF:
            return "void*";
        case IL2CPP_TYPE_VAR:
        case IL2CPP_TYPE_MVAR:
            return "void*";
        default:
            return "void*";
    }
}

// ==================== dump.cs helpers (original) ====================

std::string get_method_modifier(uint32_t flags) {
    std::stringstream outPut;
    auto access = flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (access) {
        case METHOD_ATTRIBUTE_PRIVATE:
            outPut << "private ";
            break;
        case METHOD_ATTRIBUTE_PUBLIC:
            outPut << "public ";
            break;
        case METHOD_ATTRIBUTE_FAMILY:
            outPut << "protected ";
            break;
        case METHOD_ATTRIBUTE_ASSEM:
        case METHOD_ATTRIBUTE_FAM_AND_ASSEM:
            outPut << "internal ";
            break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & METHOD_ATTRIBUTE_STATIC) {
        outPut << "static ";
    }
    if (flags & METHOD_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_FINAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "sealed override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_VIRTUAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT) {
            outPut << "virtual ";
        } else {
            outPut << "override ";
        }
    }
    if (flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) {
        outPut << "extern ";
    }
    return outPut.str();
}

bool _il2cpp_type_is_byref(const Il2CppType *type) {
    auto byref = type->byref;
    if (il2cpp_type_is_byref) {
        byref = il2cpp_type_is_byref(type);
    }
    return byref;
}

std::string dump_method(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Methods\n";
    void *iter = nullptr;
    while (auto method = il2cpp_class_get_methods(klass, &iter)) {
        if (method->methodPointer) {
            outPut << "\t// RVA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer - il2cpp_base;
            outPut << " VA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer;
        } else {
            outPut << "\t// RVA: 0x VA: 0x0";
        }
        outPut << "\n\t";
        uint32_t iflags = 0;
        auto flags = il2cpp_method_get_flags(method, &iflags);
        outPut << get_method_modifier(flags);
        auto return_type = il2cpp_method_get_return_type(method);
        if (_il2cpp_type_is_byref(return_type)) {
            outPut << "ref ";
        }
        auto return_class = il2cpp_class_from_type(return_type);
        outPut << il2cpp_class_get_name(return_class) << " " << il2cpp_method_get_name(method)
               << "(";
        auto param_count = il2cpp_method_get_param_count(method);
        for (int i = 0; i < param_count; ++i) {
            auto param = il2cpp_method_get_param(method, i);
            auto attrs = param->attrs;
            if (_il2cpp_type_is_byref(param)) {
                if (attrs & PARAM_ATTRIBUTE_OUT && !(attrs & PARAM_ATTRIBUTE_IN)) {
                    outPut << "out ";
                } else if (attrs & PARAM_ATTRIBUTE_IN && !(attrs & PARAM_ATTRIBUTE_OUT)) {
                    outPut << "in ";
                } else {
                    outPut << "ref ";
                }
            } else {
                if (attrs & PARAM_ATTRIBUTE_IN) {
                    outPut << "[In] ";
                }
                if (attrs & PARAM_ATTRIBUTE_OUT) {
                    outPut << "[Out] ";
                }
            }
            auto parameter_class = il2cpp_class_from_type(param);
            outPut << il2cpp_class_get_name(parameter_class) << " "
                   << il2cpp_method_get_param_name(method, i);
            outPut << ", ";
        }
        if (param_count > 0) {
            outPut.seekp(-2, outPut.cur);
        }
        outPut << ") { }\n";
    }
    return outPut.str();
}

std::string dump_property(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Properties\n";
    void *iter = nullptr;
    while (auto prop_const = il2cpp_class_get_properties(klass, &iter)) {
        auto prop = const_cast<PropertyInfo *>(prop_const);
        auto get = il2cpp_property_get_get_method(prop);
        auto set = il2cpp_property_get_set_method(prop);
        auto prop_name = il2cpp_property_get_name(prop);
        outPut << "\t";
        Il2CppClass *prop_class = nullptr;
        uint32_t iflags = 0;
        if (get) {
            outPut << get_method_modifier(il2cpp_method_get_flags(get, &iflags));
            prop_class = il2cpp_class_from_type(il2cpp_method_get_return_type(get));
        } else if (set) {
            outPut << get_method_modifier(il2cpp_method_get_flags(set, &iflags));
            auto param = il2cpp_method_get_param(set, 0);
            prop_class = il2cpp_class_from_type(param);
        }
        if (prop_class) {
            outPut << il2cpp_class_get_name(prop_class) << " " << prop_name << " { ";
            if (get) {
                outPut << "get; ";
            }
            if (set) {
                outPut << "set; ";
            }
            outPut << "}\n";
        } else {
            if (prop_name) {
                outPut << " // unknown property " << prop_name;
            }
        }
    }
    return outPut.str();
}

std::string dump_field(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Fields\n";
    auto is_enum = il2cpp_class_is_enum(klass);
    void *iter = nullptr;
    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        outPut << "\t";
        auto attrs = il2cpp_field_get_flags(field);
        auto access = attrs & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK;
        switch (access) {
            case FIELD_ATTRIBUTE_PRIVATE:
                outPut << "private ";
                break;
            case FIELD_ATTRIBUTE_PUBLIC:
                outPut << "public ";
                break;
            case FIELD_ATTRIBUTE_FAMILY:
                outPut << "protected ";
                break;
            case FIELD_ATTRIBUTE_ASSEMBLY:
            case FIELD_ATTRIBUTE_FAM_AND_ASSEM:
                outPut << "internal ";
                break;
            case FIELD_ATTRIBUTE_FAM_OR_ASSEM:
                outPut << "protected internal ";
                break;
        }
        if (attrs & FIELD_ATTRIBUTE_LITERAL) {
            outPut << "const ";
        } else {
            if (attrs & FIELD_ATTRIBUTE_STATIC) {
                outPut << "static ";
            }
            if (attrs & FIELD_ATTRIBUTE_INIT_ONLY) {
                outPut << "readonly ";
            }
        }
        auto field_type = il2cpp_field_get_type(field);
        auto field_class = il2cpp_class_from_type(field_type);
        outPut << il2cpp_class_get_name(field_class) << " " << il2cpp_field_get_name(field);
        if (attrs & FIELD_ATTRIBUTE_LITERAL && is_enum) {
            uint64_t val = 0;
            il2cpp_field_static_get_value(field, &val);
            outPut << " = " << std::dec << val;
        }
        outPut << "; // 0x" << std::hex << il2cpp_field_get_offset(field) << "\n";
    }
    return outPut.str();
}

std::string dump_type(const Il2CppType *type) {
    std::stringstream outPut;
    auto *klass = il2cpp_class_from_type(type);
    outPut << "\n// Namespace: " << il2cpp_class_get_namespace(klass) << "\n";
    auto flags = il2cpp_class_get_flags(klass);
    if (flags & TYPE_ATTRIBUTE_SERIALIZABLE) {
        outPut << "[Serializable]\n";
    }
    auto is_valuetype = il2cpp_class_is_valuetype(klass);
    auto is_enum = il2cpp_class_is_enum(klass);
    auto visibility = flags & TYPE_ATTRIBUTE_VISIBILITY_MASK;
    switch (visibility) {
        case TYPE_ATTRIBUTE_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_PUBLIC:
            outPut << "public ";
            break;
        case TYPE_ATTRIBUTE_NOT_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM:
        case TYPE_ATTRIBUTE_NESTED_ASSEMBLY:
            outPut << "internal ";
            break;
        case TYPE_ATTRIBUTE_NESTED_PRIVATE:
            outPut << "private ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAMILY:
            outPut << "protected ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & TYPE_ATTRIBUTE_ABSTRACT && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "static ";
    } else if (!(flags & TYPE_ATTRIBUTE_INTERFACE) && flags & TYPE_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
    } else if (!is_valuetype && !is_enum && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "sealed ";
    }
    if (flags & TYPE_ATTRIBUTE_INTERFACE) {
        outPut << "interface ";
    } else if (is_enum) {
        outPut << "enum ";
    } else if (is_valuetype) {
        outPut << "struct ";
    } else {
        outPut << "class ";
    }
    outPut << il2cpp_class_get_name(klass);
    std::vector<std::string> extends;
    auto parent = il2cpp_class_get_parent(klass);
    if (!is_valuetype && !is_enum && parent) {
        auto parent_type = il2cpp_class_get_type(parent);
        if (parent_type->type != IL2CPP_TYPE_OBJECT) {
            extends.emplace_back(il2cpp_class_get_name(parent));
        }
    }
    void *iter = nullptr;
    while (auto itf = il2cpp_class_get_interfaces(klass, &iter)) {
        extends.emplace_back(il2cpp_class_get_name(itf));
    }
    if (!extends.empty()) {
        outPut << " : " << extends[0];
        for (int i = 1; i < extends.size(); ++i) {
            outPut << ", " << extends[i];
        }
    }
    outPut << "\n{";
    outPut << dump_field(klass);
    outPut << dump_property(klass);
    outPut << dump_method(klass);
    outPut << "}\n";
    return outPut.str();
}

// ==================== script.json generation ====================

struct ScriptMethod {
    uint64_t address;
    std::string name;
    std::string signature;
};

static void collect_methods_for_script(Il2CppClass *klass, const char *imageName,
                                        std::vector<ScriptMethod> &scriptMethods) {
    auto className = il2cpp_class_get_name(klass);
    auto ns = il2cpp_class_get_namespace(klass);
    std::string fullClassName;
    if (ns && strlen(ns) > 0) {
        fullClassName = std::string(ns) + "." + className;
    } else {
        fullClassName = className ? className : "";
    }

    void *iter = nullptr;
    while (auto method = il2cpp_class_get_methods(klass, &iter)) {
        ScriptMethod sm;
        if (method->methodPointer) {
            sm.address = (uint64_t) method->methodPointer - il2cpp_base;
        } else {
            sm.address = 0;
        }

        auto methodName = il2cpp_method_get_name(method);
        sm.name = fullClassName + "$$" + (methodName ? methodName : "");

        // Build signature
        std::stringstream sig;
        auto return_type = il2cpp_method_get_return_type(method);
        auto return_class = il2cpp_class_from_type(return_type);
        sig << il2cpp_class_get_name(return_class) << " " << fullClassName
            << "::" << (methodName ? methodName : "") << "(";
        auto param_count = il2cpp_method_get_param_count(method);
        for (int i = 0; i < param_count; ++i) {
            auto param = il2cpp_method_get_param(method, i);
            auto parameter_class = il2cpp_class_from_type(param);
            if (i > 0) sig << ", ";
            sig << il2cpp_class_get_name(parameter_class);
        }
        sig << ")";
        sm.signature = sig.str();

        scriptMethods.push_back(sm);
    }
}

static void write_script_json(const std::string &outPath,
                               const std::vector<ScriptMethod> &scriptMethods) {
    LOGI("writing script.json...");
    std::ofstream out(outPath);
    out << "{\n";

    // ScriptMethod
    out << "  \"ScriptMethod\": [\n";
    for (size_t i = 0; i < scriptMethods.size(); ++i) {
        auto &m = scriptMethods[i];
        out << "    {\"Address\": " << std::dec << m.address
            << ", \"Name\": \"" << json_escape(m.name)
            << "\", \"Signature\": \"" << json_escape(m.signature) << "\"}";
        if (i + 1 < scriptMethods.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";

    // ScriptString - requires metadata access not available via runtime API
    out << "  \"ScriptString\": [],\n";

    // ScriptMetadata - requires metadata access not available via runtime API
    out << "  \"ScriptMetadata\": [],\n";

    // ScriptMetadataMethod - requires metadata access not available via runtime API
    out << "  \"ScriptMetadataMethod\": [],\n";

    // Addresses - unique sorted list of all method RVAs
    out << "  \"Addresses\": [\n";
    std::vector<uint64_t> addresses;
    for (auto &m : scriptMethods) {
        if (m.address != 0) {
            addresses.push_back(m.address);
        }
    }
    std::sort(addresses.begin(), addresses.end());
    addresses.erase(std::unique(addresses.begin(), addresses.end()), addresses.end());
    for (size_t i = 0; i < addresses.size(); ++i) {
        out << "    " << std::dec << addresses[i];
        if (i + 1 < addresses.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";

    out << "}\n";
    out.close();
    LOGI("script.json done!");
}

// ==================== il2cpp.h generation ====================

static void write_il2cpp_header(const std::string &outPath,
                                 const Il2CppAssembly **assemblies, size_t assemblyCount) {
    LOGI("writing il2cpp.h...");
    std::ofstream out(outPath);

    out << "// Generated by Zygisk-Il2CppDumper\n";
    out << "// il2cpp.h - Struct definitions with field offsets\n\n";
    out << "#pragma once\n\n";
    out << "#include <stdint.h>\n";
    out << "#include <stdbool.h>\n\n";

    // Base types
    out << "// ==================== Base Types ====================\n\n";
    out << "typedef struct Il2CppObject {\n";
    out << "    void* klass;\n";
    out << "    void* monitor;\n";
    out << "} Il2CppObject;\n\n";

    out << "typedef struct Il2CppString {\n";
    out << "    Il2CppObject object;\n";
    out << "    int32_t length;\n";
    out << "    uint16_t chars[1];\n";
    out << "} Il2CppString;\n\n";

    out << "typedef struct Il2CppArray {\n";
    out << "    Il2CppObject object;\n";
    out << "    void* bounds;\n";
    out << "    uintptr_t max_length;\n";
    out << "    void* vector[1];\n";
    out << "} Il2CppArray;\n\n";

    out << "// ==================== Game Types ====================\n\n";

    if (!il2cpp_image_get_class) {
        out << "// il2cpp_image_get_class not available (version < 2018.3)\n";
        out << "// Struct generation requires il2cpp version >= 2018.3\n";
        out.close();
        LOGI("il2cpp.h done (limited - old version)");
        return;
    }

    for (size_t i = 0; i < assemblyCount; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        auto imageName = il2cpp_image_get_name(image);
        auto classCount = il2cpp_image_get_class_count(image);

        out << "// ==========================================================\n";
        out << "// Assembly: " << imageName << " (" << std::dec << classCount << " classes)\n";
        out << "// ==========================================================\n\n";

        for (size_t j = 0; j < classCount; ++j) {
            auto klass = const_cast<Il2CppClass *>(il2cpp_image_get_class(image, j));
            auto className = il2cpp_class_get_name(klass);
            auto ns = il2cpp_class_get_namespace(klass);
            auto flags = il2cpp_class_get_flags(klass);

            if (!className || strlen(className) == 0) continue;

            std::string fullName = get_full_name(klass);
            std::string safeName = sanitize_name(fullName);

            auto is_valuetype = il2cpp_class_is_valuetype(klass);
            auto is_enum = il2cpp_class_is_enum(klass);
            auto is_interface = (flags & TYPE_ATTRIBUTE_INTERFACE) != 0;

            if (is_interface) continue;

            if (ns && strlen(ns) > 0) {
                out << "// Namespace: " << ns << "\n";
            }

            // Collect fields
            std::vector<std::pair<std::string, std::pair<std::string, size_t>>> instanceFields;
            std::vector<std::pair<std::string, std::string>> staticFields;

            void *iter = nullptr;
            while (auto field = il2cpp_class_get_fields(klass, &iter)) {
                auto attrs = il2cpp_field_get_flags(field);
                auto field_type = il2cpp_field_get_type(field);
                auto field_name_cstr = il2cpp_field_get_name(field);
                auto offset = il2cpp_field_get_offset(field);

                std::string field_name = field_name_cstr ? field_name_cstr : "_unknown";
                std::string c_type = il2cpp_type_to_c(field_type, true);
                std::string safe_field = sanitize_name(field_name);

                if (attrs & FIELD_ATTRIBUTE_LITERAL) continue;

                if (attrs & FIELD_ATTRIBUTE_STATIC) {
                    staticFields.push_back({c_type, safe_field});
                } else {
                    instanceFields.push_back({c_type, {safe_field, offset}});
                }
            }

            // Enum
            if (is_enum) {
                out << "enum " << safeName << " {\n";
                void *eiter = nullptr;
                while (auto field = il2cpp_class_get_fields(klass, &eiter)) {
                    auto attrs = il2cpp_field_get_flags(field);
                    if (!(attrs & FIELD_ATTRIBUTE_LITERAL)) continue;
                    auto field_name_cstr = il2cpp_field_get_name(field);
                    if (!field_name_cstr) continue;
                    uint64_t val = 0;
                    il2cpp_field_static_get_value(field, &val);
                    out << "    " << sanitize_name(field_name_cstr)
                        << " = " << std::dec << val << ",\n";
                }
                out << "};\n\n";
                continue;
            }

            // Fields struct
            if (!instanceFields.empty()) {
                out << "struct " << safeName << "_Fields {\n";
                for (auto &f : instanceFields) {
                    out << "    " << f.first << " " << f.second.first
                        << "; // 0x" << std::hex << f.second.second << "\n";
                }
                out << "};\n\n";
            }

            // Static fields struct
            if (!staticFields.empty()) {
                out << "struct " << safeName << "_StaticFields {\n";
                for (auto &f : staticFields) {
                    out << "    " << f.first << " " << f.second << ";\n";
                }
                out << "};\n\n";
            }

            // Object struct
            if (is_valuetype) {
                out << "struct " << safeName << "_o {\n";
                if (!instanceFields.empty()) {
                    out << "    struct " << safeName << "_Fields fields;\n";
                }
                out << "};\n\n";
            } else {
                out << "struct " << safeName << "_o {\n";
                out << "    void* klass;\n";
                out << "    void* monitor;\n";
                if (!instanceFields.empty()) {
                    out << "    struct " << safeName << "_Fields fields;\n";
                }
                out << "};\n\n";
            }

            // Method declarations as comments
            void *miter = nullptr;
            bool hasMethodHeader = false;
            while (auto method = il2cpp_class_get_methods(klass, &miter)) {
                if (!hasMethodHeader) {
                    out << "// " << safeName << " methods:\n";
                    hasMethodHeader = true;
                }
                auto methodName = il2cpp_method_get_name(method);
                uint64_t rva = 0;
                if (method->methodPointer) {
                    rva = (uint64_t)method->methodPointer - il2cpp_base;
                }

                auto return_type = il2cpp_method_get_return_type(method);
                std::string retC = il2cpp_type_to_c(return_type);

                out << "// " << retC << " " << safeName << "_"
                    << sanitize_name(methodName ? methodName : "unknown") << "(";

                uint32_t iflags = 0;
                auto mflags = il2cpp_method_get_flags(method, &iflags);
                bool isStatic = (mflags & METHOD_ATTRIBUTE_STATIC) != 0;

                bool first = true;
                if (!isStatic) {
                    out << "struct " << safeName << "_o* __this";
                    first = false;
                }

                auto param_count = il2cpp_method_get_param_count(method);
                for (int p = 0; p < param_count; ++p) {
                    if (!first) out << ", ";
                    first = false;
                    auto param = il2cpp_method_get_param(method, p);
                    auto pname = il2cpp_method_get_param_name(method, p);
                    out << il2cpp_type_to_c(param) << " " << (pname ? pname : "param");
                }

                out << "); // RVA: 0x" << std::hex << rva << "\n";
            }
            if (hasMethodHeader) out << "\n";
        }
    }

    out.close();
    LOGI("il2cpp.h done!");
}

// ==================== Per-assembly dump files (DummyDll) ====================

static void write_per_assembly_dumps(const std::string &dummyDllDir,
                                      const Il2CppAssembly **assemblies, size_t assemblyCount) {
    LOGI("writing per-assembly dumps...");
    mkdir(dummyDllDir.c_str(), 0777);

    if (!il2cpp_image_get_class) {
        LOGW("il2cpp_image_get_class not available, skipping per-assembly dumps");
        return;
    }

    for (size_t i = 0; i < assemblyCount; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        auto imageName = il2cpp_image_get_name(image);
        auto classCount = il2cpp_image_get_class_count(image);

        std::string fileName = dummyDllDir + "/" + imageName + ".txt";
        std::ofstream out(fileName);

        out << "// Assembly: " << imageName << "\n";
        out << "// Class count: " << std::dec << classCount << "\n";
        out << "// Generated by Zygisk-Il2CppDumper\n\n";

        for (size_t j = 0; j < classCount; ++j) {
            auto klass = const_cast<Il2CppClass *>(il2cpp_image_get_class(image, j));
            auto type = il2cpp_class_get_type(klass);
            out << dump_type(type);
        }

        out.close();
    }
    LOGI("per-assembly dumps done!");
}

// ==================== Main API ====================

void il2cpp_api_init(void *handle) {
    LOGI("il2cpp_handle: %p", handle);
    init_il2cpp_api(handle);
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        }
        LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return;
    }
    while (!il2cpp_is_vm_thread(nullptr)) {
        LOGI("Waiting for il2cpp_init...");
        sleep(1);
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
}

void il2cpp_dump(const char *outDir) {
    LOGI("dumping...");
    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);

    // Create output directories
    std::string filesDir = std::string(outDir) + "/files";
    mkdir(filesDir.c_str(), 0777);

    std::string dummyDllDir = filesDir + "/DummyDll";

    std::stringstream imageOutput;
    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        imageOutput << "// Image " << i << ": " << il2cpp_image_get_name(image) << "\n";
    }

    // ===== Collect all data =====
    std::vector<std::string> outPuts;
    std::vector<ScriptMethod> scriptMethods;

    if (il2cpp_image_get_class) {
        LOGI("Version greater than 2018.3");
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            auto imageName = il2cpp_image_get_name(image);
            imageStr << "\n// Dll : " << imageName;
            auto classCount = il2cpp_image_get_class_count(image);
            for (int j = 0; j < classCount; ++j) {
                auto klass = const_cast<Il2CppClass *>(il2cpp_image_get_class(image, j));
                auto type = il2cpp_class_get_type(klass);
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);

                // Collect methods for script.json
                collect_methods_for_script(klass, imageName, scriptMethods);
            }
        }
    } else {
        LOGI("Version less than 2018.3");
        auto corlib = il2cpp_get_corlib();
        auto assemblyClass = il2cpp_class_from_name(corlib, "System.Reflection", "Assembly");
        auto assemblyLoad = il2cpp_class_get_method_from_name(assemblyClass, "Load", 1);
        auto assemblyGetTypes = il2cpp_class_get_method_from_name(assemblyClass, "GetTypes", 0);
        if (assemblyLoad && assemblyLoad->methodPointer) {
            LOGI("Assembly::Load: %p", assemblyLoad->methodPointer);
        } else {
            LOGI("miss Assembly::Load");
            return;
        }
        if (assemblyGetTypes && assemblyGetTypes->methodPointer) {
            LOGI("Assembly::GetTypes: %p", assemblyGetTypes->methodPointer);
        } else {
            LOGI("miss Assembly::GetTypes");
            return;
        }
        typedef void *(*Assembly_Load_ftn)(void *, Il2CppString *, void *);
        typedef Il2CppArray *(*Assembly_GetTypes_ftn)(void *, void *);
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            auto image_name = il2cpp_image_get_name(image);
            imageStr << "\n// Dll : " << image_name;
            auto imageName = std::string(image_name);
            auto pos = imageName.rfind('.');
            auto imageNameNoExt = imageName.substr(0, pos);
            auto assemblyFileName = il2cpp_string_new(imageNameNoExt.data());
            auto reflectionAssembly = ((Assembly_Load_ftn) assemblyLoad->methodPointer)(nullptr,
                                                                                        assemblyFileName,
                                                                                        nullptr);
            auto reflectionTypes = ((Assembly_GetTypes_ftn) assemblyGetTypes->methodPointer)(
                    reflectionAssembly, nullptr);
            auto items = reflectionTypes->vector;
            for (int j = 0; j < reflectionTypes->max_length; ++j) {
                auto klass = il2cpp_class_from_system_type((Il2CppReflectionType *) items[j]);
                auto type = il2cpp_class_get_type(klass);
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);

                // Collect methods for script.json
                collect_methods_for_script(klass, image_name, scriptMethods);
            }
        }
    }

    // ===== 1. Write dump.cs =====
    LOGI("write dump.cs");
    auto dumpPath = filesDir + "/dump.cs";
    std::ofstream outStream(dumpPath);
    outStream << imageOutput.str();
    auto count = outPuts.size();
    for (int i = 0; i < count; ++i) {
        outStream << outPuts[i];
    }
    outStream.close();
    LOGI("dump.cs done!");

    // ===== 2. Write script.json =====
    auto scriptPath = filesDir + "/script.json";
    write_script_json(scriptPath, scriptMethods);

    // ===== 3. Write il2cpp.h =====
    auto headerPath = filesDir + "/il2cpp.h";
    write_il2cpp_header(headerPath, assemblies, size);

    // ===== 4. Write per-assembly dumps (DummyDll) =====
    write_per_assembly_dumps(dummyDllDir, assemblies, size);

    LOGI("=== All dumps complete! ===");
    LOGI("Output files:");
    LOGI("  - %s/dump.cs", filesDir.c_str());
    LOGI("  - %s/script.json", filesDir.c_str());
    LOGI("  - %s/il2cpp.h", filesDir.c_str());
    LOGI("  - %s/DummyDll/  (per-assembly)", dummyDllDir.c_str());
}
