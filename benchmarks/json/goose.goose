import std;
import json;

let REQUESTS = 100000;
let PAYLOAD = "{\"id\":42,\"workers\":128,\"enabled\":true,\"service\":\"swan\",\"items\":[1,2,3,4,5],\"meta\":{\"region\":\"ap-southeast-1\",\"version\":1}}";

recursive fn render(node: json::Node&, out: u8[>..]&) {
    match node.val {
        Null => out.append("null"),
        Bool b => json::format_bool(out, b.v),
        Num n => {
            let integer = n.v as! i64;
            if integer == n.v { json::format_int(out, integer); }
            else { json::format_number(out, n.v); }
        },
        Str s => json::format_string(out, s.s),
        Arr &a => render_children(a.first, false, out, '[', ']'),
        Obj &o => render_children(o.first, true, out, '{', '}'),
    }
}

recursive fn render_children(first: json::Node?, keyed: bool,
                             out: u8[>..]&, open: u8, close: u8) {
    out.push(open);
    var child = first;
    var index = 0;
    while child {
        if index > 0 { out.push(','); }
        if keyed { json::format_key(out, child.key); }
        render(child, out);
        child .= child.next;
        index++;
    }
    out.push(close);
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
