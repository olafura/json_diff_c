curl -o profile-data/cdc.json https://data.cdc.gov/api/views/25m4-6qqq/rows.json?accessType=DOWNLOAD
curl -o profile-data/edg.json https://edg.epa.gov/data.json
#!/bin/bash
# Download medium-sized JSON files for profiling tests

mkdir -p profile-data

echo "Downloading medium profile data..."
curl -o profile-data/cdc.json https://data.cdc.gov/api/views/25m4-6qqq/rows.json?accessType=DOWNLOAD || {
    echo "Failed to download CDC data, creating fallback..."
    echo '{"data": [["row1", "value1"], ["row2", "value2"]], "meta": {"view": {"name": "CDC Test Data"}}}' > profile-data/cdc.json
}

curl -o profile-data/edg.json https://edg.epa.gov/data.json || {
    echo "Failed to download EPA data, creating fallback..."
    echo '{"dataset": [{"title": "EPA Test Data", "description": "Test dataset"}], "conformsTo": "https://project-open-data.cio.gov/v1.1/schema"}' > profile-data/edg.json
}

echo "Medium profile data ready."
