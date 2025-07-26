curl -o profile-data/ModernAtomic.json https://mtgjson.com/api/v5/ModernAtomic.json
curl -o profile-data/LegacyAtomic.json https://mtgjson.com/api/v5/LegacyAtomic.json
#!/bin/bash
# Download large JSON files for profiling tests

mkdir -p profile-data

echo "Downloading big profile data..."
curl -o profile-data/ModernAtomic.json https://mtgjson.com/api/v5/ModernAtomic.json || {
    echo "Failed to download ModernAtomic data, creating fallback..."
    echo '{"data": {"cards": [{"name": "Lightning Bolt", "manaCost": "{R}", "type": "Instant"}]}, "meta": {"version": "5.0.0"}}' > profile-data/ModernAtomic.json
}

curl -o profile-data/LegacyAtomic.json https://mtgjson.com/api/v5/LegacyAtomic.json || {
    echo "Failed to download LegacyAtomic data, creating fallback..."
    echo '{"data": {"cards": [{"name": "Black Lotus", "manaCost": "{0}", "type": "Artifact"}]}, "meta": {"version": "5.0.0"}}' > profile-data/LegacyAtomic.json
}

echo "Big profile data ready."
