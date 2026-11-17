import std;
import json;

let REQUESTS = 100000;
let PAYLOAD = "{\"id\":42,\"workers\":128,\"enabled\":true,\"service\":\"swan\",\"items\":[1,2,3,4,5],\"meta\":{\"region\":\"ap-southeast-1\",\"version\":1}}";

recursive fn render(node: json::Node&, out: u8[>..]&) {
    match node.val {
        Null => out.append("null"),
        Bool b => json::format_bool(out, b.v),
        Int n => json::format_int(out, n.v),
        Num n => json::format_number(out, n.v),
        Str s => json::format_string(out, s.s),
        Arr a => {
            out.push('[');
            var child = node.first;
            var index = 0;
            while child {
                if index > 0 { out.push(','); }
                render(child, out);
                child .= child.next;
                index++;
            }
            out.push(']');
        },
        Obj o => {
            out.push('{');
            var child = node.first;
            var index = 0;
            while child {
                if index > 0 { out.push(','); }
                json::format_key(out, child.key);
                render(child, out);
                child .= child.next;
                index++;
            }
            out.push('}');
        },
    }
}

fn main() {
    var doc: json::Node[>..] = [];
    var decoded: u8[>..] = [];
    var out: u8[>..] = [];
    var request: i64 = 0;
    var checksum: i64 = 0;
    var bytes: i64 = 0;
    while request < REQUESTS {
        doc.clear();
        decoded.clear();
        out.clear();
        let root, err = json::parse(PAYLOAD, doc, decoded);
        guard root else { abort(err); }
        let id, id_ok = json::as_i64(json::member(root, "id"));
        let workers, workers_ok = json::as_i64(json::member(root, "workers"));
        if !id_ok || !workers_ok || id != 42 || workers != 128 {
            abort("JSON benchmark value mismatch");
        }
        render(root, out);
        checksum += id + workers;
        bytes += PAYLOAD.len + out.len;
        request++;
    }
    print(checksum);
    print(bytes);
}
