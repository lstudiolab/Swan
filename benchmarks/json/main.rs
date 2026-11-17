use serde_json::Value;

const REQUESTS: usize = 100_000;
const PAYLOAD: &str = r#"{"id":42,"workers":128,"enabled":true,"service":"swan","items":[1,2,3,4,5],"meta":{"region":"ap-southeast-1","version":1}}"#;

fn main() {
    let input = PAYLOAD.as_bytes();
    let mut checksum: i64 = 0;
    let mut total_bytes: i64 = 0;
    for _ in 0..REQUESTS {
        let value: Value = serde_json::from_slice(input).expect("valid JSON");
        let id = value["id"].as_i64().expect("id");
        let workers = value["workers"].as_i64().expect("workers");
        assert_eq!(id, 42);
        assert_eq!(workers, 128);
        let output = serde_json::to_vec(&value).expect("encode");
        checksum += id + workers;
        total_bytes += (input.len() + output.len()) as i64;
    }
    println!("{checksum}");
    println!("{total_bytes}");
}
