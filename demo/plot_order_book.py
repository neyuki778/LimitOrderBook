import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns # Using seaborn for potentially nicer plots

# --- Configuration ---
CSV_FILENAME = r'E:/Project/OrderBook-master/demo/sample_operations.csv'
# Set plot style
sns.set_theme(style="whitegrid")

# --- Load Data ---
try:
    # Read CSV, handle potentially empty Price and Volume columns
    # Replace potential empty or non-numeric values with NaN for later processing
    df = pd.read_csv(CSV_FILENAME, na_values=[''])

    # Check if necessary columns exist and rename them
    if 'Type' in df.columns:
        df.rename(columns={'Type': 'Side'}, inplace=True) # Rename 'Type' column to 'Side'
    else:
        # If even the 'Type' column is missing, the CSV file format might be incorrect
        print("Error: Missing 'Type' column in the CSV file.")
        exit()

    # Data type conversion, fill NaN for failed conversions
    df['Order ID'] = pd.to_numeric(df['Order ID'], errors='coerce')
    # Now 'Side' column can be safely accessed
    # Side might be empty for DELETE (previously set to ''), convert to float then Int64 (supports NaN)
    df['Side'] = pd.to_numeric(df['Side'], errors='coerce').astype('Int64')
    df['Price'] = pd.to_numeric(df['Price'], errors='coerce')
    df['Volume'] = pd.to_numeric(df['Volume'], errors='coerce')

    # Check if Timestamp column exists, skip processing if not
    if 'Timestamp' in df.columns:
         df['Timestamp'] = pd.to_numeric(df['Timestamp'], errors='coerce')
    else:
        print("Warning: Missing 'Timestamp' column in the CSV file, time-related analysis will be skipped.")


    # Delete rows with NaN (if strict processing is needed) or fill based on circumstances
    # Here we mainly analyze non-DELETE operations, which usually have values
    print(f"Loading complete, {len(df)} records.")
    print("Data preview:")
    print(df.head())
    print("\nData types:")
    print(df.dtypes)
    print("\nOperation type statistics:")
    print(df['Operation'].value_counts())

except FileNotFoundError:
    print(f"Error: File not found {CSV_FILENAME}")
    exit()
except KeyError as e: # Catch KeyError more specifically
    print(f"KeyError during data loading: {e} - Please check if the CSV file header is correct.")
    exit()
except Exception as e:
    print(f"Error during data loading: {e}")
    exit()

# --- Data Analysis and Visualization ---

plt.figure(figsize=(18, 12)) # Create a large canvas for multiple subplots
plt.suptitle('Order Generation Analysis', fontsize=16)

# 1. Operation Type Distribution (Pie Chart + Count)
plt.subplot(2, 3, 1) # 1st of 2 rows and 3 columns
op_counts = df['Operation'].value_counts()
plt.pie(op_counts, labels=op_counts.index.tolist(), autopct='%1.1f%%', startangle=90, colors=sns.color_palette("pastel"))
plt.title('Operation Type Distribution')

# 2. Buy/Sell Side Distribution (PLACE and MARKET only)
plt.subplot(2, 3, 2)
place_market_df = df[df['Operation'].isin(['PLACE', 'MARKET'])].copy() # Filter for PLACE and MARKET
# Map Side 0/1 to Buy/Sell
place_market_df['Side_Label'] = place_market_df['Side'].map({0: 'Buy', 1: 'Sell'})
side_counts = place_market_df['Side_Label'].value_counts()
sns.barplot(x=side_counts.index.tolist(), y=side_counts.values, palette="viridis") # Changed to use .tolist()
plt.title('PLACE/MARKET Orders Buy/Sell Side')
plt.ylabel('Count')
plt.xlabel('Side')

# 3. Limit Order (PLACE) Price Distribution (Histogram)
plt.subplot(2, 3, 3)
limit_orders_df = df[df['Operation'] == 'PLACE'].dropna(subset=['Price']) # Filter PLACE with non-empty Price
if not limit_orders_df.empty:
    sns.histplot(x=limit_orders_df['Price'], kde=True, bins=50, color='skyblue')
    plt.title('Limit Order (PLACE) Price Distribution')
    plt.xlabel('Price')
    plt.ylabel('Frequency')
else:
    plt.title('No valid Limit Order Price data')

# 4. PLACE/MARKET Order Volume Distribution (Histogram - Log Scale)
plt.subplot(2, 3, 4)
volume_df = df[df['Operation'].isin(['PLACE', 'MARKET'])].dropna(subset=['Volume']) # Filter PLACE/MARKET with non-empty Volume
if not volume_df.empty and volume_df['Volume'].nunique() > 1: # Ensure there is data and more than one value
    # Using log scale might better show the distribution, especially with power law tails
    # Use np.log1p to handle 0 or negative values (although volume > 0 here)
    sns.histplot(np.log1p(volume_df['Volume']), kde=True, bins=50, color='lightcoral')
    plt.title('PLACE/MARKET Order Volume Distribution (Log Scale)')
    plt.xlabel('Log(1 + Volume)')
    plt.ylabel('Frequency')
    # Remind the user that the X-axis is on a log scale
    plt.text(0.1, 0.9, 'X-axis is Log Scale', transform=plt.gca().transAxes, ha='left', va='top', fontsize=8, color='red')
elif not volume_df.empty:
     sns.histplot(x=np.log1p(volume_df['Volume']), kde=True, bins=50, color='lightcoral') # If few values, show directly
     plt.title('PLACE/MARKET Order Volume Distribution')
     plt.xlabel('Volume')
     plt.ylabel('Frequency')
else:
    plt.title('No valid Volume data')


# 5. Limit Order (PLACE) Price vs Volume (Scatter Plot - Can be dense, sample or use hexbin)
plt.subplot(2, 3, 5)
place_volume_price_df = df[(df['Operation'] == 'PLACE') & df['Price'].notna() & df['Volume'].notna()]
if not place_volume_price_df.empty:
    # If there are too many points, the scatter plot will be slow and unclear, consider sampling or using hexbin
    if len(place_volume_price_df) > 5000:
        sample_df = place_volume_price_df.sample(n=5000, random_state=1)
        sns.scatterplot(data=sample_df, x='Price', y='Volume', hue='Side', alpha=0.5, s=10) # Adjust point size with s
        plt.title('Limit Order Price vs Volume (5000 point sample)')
    else:
        sns.scatterplot(data=place_volume_price_df, x='Price', y='Volume', hue='Side', alpha=0.6, s=15)
        plt.title('Limit Order Price vs Volume')

    plt.xlabel('Price')
    plt.ylabel('Volume')
    # May need to use log scale for the Y-axis
    if volume_df['Volume'].max() / volume_df['Volume'].min() > 100: # If range is large
         plt.yscale('log')
         plt.ylabel('Volume (Log Scale)')

else:
    plt.title('No valid Limit Order Price/Volume data')


# 6. Volume Distribution by Operation Type (Box Plot)
plt.subplot(2, 3, 6)
# Filter for PLACE and MARKET operations with non-empty Volume
place_market_volume_df = df[df['Operation'].isin(['PLACE', 'MARKET']) & df['Volume'].notna()].copy()

if not place_market_volume_df.empty:
    # Use a box plot to show Volume distribution for different Operations
    sns.boxplot(data=place_market_volume_df, x='Operation', y='Volume', palette='pastel')
    plt.title('Limit Order(PLACE) vs Market Order(MARKET) Volume Distribution')
    plt.xlabel('Operation Type')
    plt.ylabel('Volume')
    # If the volume range is large, using a log scale for the Y-axis might be clearer
    y_max = place_market_volume_df['Volume'].max()
    y_min = place_market_volume_df[place_market_volume_df['Volume'] > 0]['Volume'].min() # Find the smallest positive number
    if pd.notna(y_max) and pd.notna(y_min) and y_max / y_min > 100: # Check if range is large
        plt.yscale('log')
        plt.ylabel('Volume (Log Scale)')
        plt.title('Limit Order vs Market Order Volume Distribution (Log Scale)')

else:
    plt.title('No PLACE/MARKET Volume data')


# --- Display Plots ---
plt.tight_layout(rect=(0, 0.03, 1, 0.95)) # Adjust layout to prevent title overlap
plt.show()

print("\nVisualization complete.")
